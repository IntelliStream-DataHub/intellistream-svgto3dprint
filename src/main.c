#include "app.h"

#include <stdio.h>

int cli_main(int argc, char **argv, app_state *a);

int main(int argc, char **argv)
{
    app_state app;
    int rc;
    app_init(&app);
    rc = cli_main(argc, argv, &app);
    if (rc >= 0) {
        app_free(&app);
        return rc;
    }
    rc = gui_main(&app);
    app_free(&app);
    return rc;
}
