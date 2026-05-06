#include "app_window.h"
#include <gtkmm/application.h>

int main(int argc, char *argv[])
{
    auto app = Gtk::Application::create("org.ritmmedical.inventorynumber");
    return app->make_window_and_run<AppWindow>(argc, argv);
}
