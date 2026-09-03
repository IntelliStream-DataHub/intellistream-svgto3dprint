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
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      # SDL's configure aborts on Linux when an X11 extension it enables by
      # default has no development package installed (typically XInput2 from
      # libxi-dev: "Couldn't find dependency package for XINPUT"). The GUI
      # only needs core X11 (or Wayland) plus OpenGL; the extensions are
      # niceties. Switch the missing ones off instead of failing, and remember
      # which, so they come back on once the package is installed and cmake
      # is re-run in the same build directory.
      # (the remembered list lives in the cache; an INTERNAL set implies FORCE,
      # so only write it at the end)
      if(NOT DEFINED LOGO3D_SDL_X11_AUTO_OFF)
        set(LOGO3D_SDL_X11_AUTO_OFF "")
      endif()
      find_path(_l3d_x11_inc NAMES X11/Xlib.h)
      set(_l3d_auto_off "")
      set(_l3d_missing_pkgs "")
      macro(logo3d_x11_ext OPT HEADER LIB PKG)
        unset(_l3d_h CACHE)
        find_file(_l3d_h NAMES "${HEADER}" HINTS "${_l3d_x11_inc}")
        set(_l3d_ok TRUE)
        if(NOT _l3d_h)
          set(_l3d_ok FALSE)
        elseif(NOT "${LIB}" STREQUAL "")
          unset(_l3d_l CACHE)
          find_library(_l3d_l NAMES "${LIB}")
          if(NOT _l3d_l)
            set(_l3d_ok FALSE)
          endif()
        endif()
        if(_l3d_ok)
          if("SDL_X11_${OPT}" IN_LIST LOGO3D_SDL_X11_AUTO_OFF)
            set(SDL_X11_${OPT} ON CACHE BOOL "" FORCE)
          endif()
        else()
          set(SDL_X11_${OPT} OFF CACHE BOOL "" FORCE)
          list(APPEND _l3d_auto_off "SDL_X11_${OPT}")
          list(APPEND _l3d_missing_pkgs "${PKG}")
        endif()
      endmacro()
      logo3d_x11_ext(XCURSOR    X11/Xcursor/Xcursor.h      Xcursor libxcursor-dev)
      logo3d_x11_ext(XDBE       X11/extensions/Xdbe.h      ""      libxext-dev)
      logo3d_x11_ext(XINPUT     X11/extensions/XInput2.h   Xi      libxi-dev)
      logo3d_x11_ext(XFIXES     X11/extensions/Xfixes.h    Xfixes  libxfixes-dev)
      logo3d_x11_ext(XRANDR     X11/extensions/Xrandr.h    Xrandr  libxrandr-dev)
      logo3d_x11_ext(XSCRNSAVER X11/extensions/scrnsaver.h Xss     libxss-dev)
      logo3d_x11_ext(XSHAPE     X11/extensions/shape.h     ""      libxext-dev)
      logo3d_x11_ext(XSYNC      X11/extensions/sync.h      ""      libxext-dev)
      logo3d_x11_ext(XTEST      X11/extensions/XTest.h     Xtst    libxtst-dev)
      # SDL's Xfixes code also needs the XInput2 header
      if("SDL_X11_XINPUT" IN_LIST _l3d_auto_off AND NOT "SDL_X11_XFIXES" IN_LIST _l3d_auto_off)
        set(SDL_X11_XFIXES OFF CACHE BOOL "" FORCE)
        list(APPEND _l3d_auto_off "SDL_X11_XFIXES")
      endif()
      set(LOGO3D_SDL_X11_AUTO_OFF "${_l3d_auto_off}" CACHE INTERNAL "SDL X11 extension options switched off automatically")
      if(_l3d_auto_off)
        list(REMOVE_DUPLICATES _l3d_missing_pkgs)
        string(REPLACE ";" " " _l3d_off_str "${_l3d_auto_off}")
        string(REPLACE ";" " " _l3d_pkgs_str "${_l3d_missing_pkgs}")
        message(STATUS "SDL3: building without ${_l3d_off_str} (no dev package found); "
                       "for the full set: sudo apt install ${_l3d_pkgs_str}")
      endif()
    endif()
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
if(WIN32)
  # the .exe icon; the window icon is compiled in from src/icon_data.h
  enable_language(RC)
  target_sources(logo3dprint PRIVATE assets/logo3dprint.rc)
endif()
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

# --- tests ----------------------------------------------------------------
add_executable(test_region tests/test_region.c src/region.c)
target_include_directories(test_region PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_region PRIVATE tess2)
if(NOT MSVC)
  target_compile_options(test_region PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_link_libraries(test_region PRIVATE m)
endif()

enable_testing()
add_test(NAME region_unit COMMAND test_region)
add_test(NAME headless_exports
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh $<TARGET_FILE:logo3dprint>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
