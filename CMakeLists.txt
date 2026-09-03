cmake_minimum_required(VERSION 3.16)
project(logo3dprint C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(LOGO3D_FETCH_SDL "Download and build SDL3 when it is not installed" ON)

# --- SDL3 -----------------------------------------------------------------
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
  if(LOGO3D_FETCH_SDL)
    message(STATUS "SDL3 not found, fetching release-3.4.16 from GitHub")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.4.16
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
  else()
    message(FATAL_ERROR "SDL3 not found. Install libsdl3-dev (or equivalent) or enable LOGO3D_FETCH_SDL.")
  endif()
endif()

# --- vendored libtess2 ----------------------------------------------------
file(GLOB TESS2_SRC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtess2/*.c)
add_library(tess2 STATIC ${TESS2_SRC})
target_include_directories(tess2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtess2)
target_compile_definitions(tess2 PRIVATE NDEBUG)
if(NOT MSVC)
  target_compile_options(tess2 PRIVATE -w)
endif()

# --- application ----------------------------------------------------------
set(CORE_SRC
  src/xml.c src/svg.c src/region.c src/model.c src/zip.c
  src/export_stl.c src/export_3mf.c src/app.c src/cli.c src/textfont.c)
set(GUI_SRC
  src/main.c src/gui.c src/render.c src/glapi.c src/nk_sdl_gl3.c src/nk_impl.c)

add_executable(logo3dprint ${CORE_SRC} ${GUI_SRC})
target_include_directories(logo3dprint PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nuklear
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
target_link_libraries(logo3dprint PRIVATE tess2 SDL3::SDL3)
if(MSVC)
  target_compile_definitions(logo3dprint PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(logo3dprint PRIVATE -Wall -Wextra -Wno-unused-parameter)
  set_source_files_properties(src/nk_impl.c PROPERTIES COMPILE_OPTIONS "-w")
  set_source_files_properties(src/textfont.c PROPERTIES COMPILE_OPTIONS "-Wno-unused-function")
  target_compile_definitions(logo3dprint PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
endif()
if(UNIX AND NOT APPLE)
  target_link_libraries(logo3dprint PRIVATE m ${CMAKE_DL_LIBS})
elseif(APPLE)
  target_link_libraries(logo3dprint PRIVATE m)
endif()

install(TARGETS logo3dprint RUNTIME DESTINATION bin)

enable_testing()
add_test(NAME headless_exports
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh $<TARGET_FILE:logo3dprint>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
