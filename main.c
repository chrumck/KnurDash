#include<fcntl.h> 
#include <gtk/gtk.h>

static void print_hello(GtkWidget* widget, gpointer   data)
{
    g_print("Hello World\n");
}

static void setBrightness(gboolean isUp)
{
    const unsigned char increment = 32;

    const int fd = open("/sys/class/backlight/10-0045/brightness", O_RDWR);
    if (fd < 0) return;

    char brString[10];
    read(fd, brString, sizeof(brString));

    int br;
    sscanf(brString, "%d", &br);

    br += isUp == TRUE ? increment : -increment;
    br = br > 255 ? 255 : br < 0 ? 0 : br;

    sprintf(brString, "%d\n", br);
    write(fd, brString, strlen(brString));

    close(fd);
}

static void setBrightnessDown() { setBrightness(FALSE); }
static void setBrightnessUp() { setBrightness(TRUE); }

static void shutDown(GtkWidget* widget, gpointer user_data)
{
#ifdef NDEBUG
    system("sudo shutdown now");
#else
    gtk_main_quit();
#endif
}

int main(int argc, char* argv[])
{
    gtk_init(&argc, &argv);


    char exeFilePath[PATH_MAX + 5];
    readlink("/proc/self/exe", exeFilePath, PATH_MAX);

    char cssFilePath[PATH_MAX + NAME_MAX];
    strcpy(cssFilePath, exeFilePath);
    strcat(cssFilePath, ".css");

    GError* error = NULL;

    GtkCssProvider* cssProvider = gtk_css_provider_new();
    if (gtk_css_provider_load_from_path(cssProvider, cssFilePath, &error) == 0)
    {
        g_printerr("Error loading file: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    GdkScreen* screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    char uiFilePath[PATH_MAX + NAME_MAX];
    strcpy(uiFilePath, exeFilePath);
    strcat(uiFilePath, ".ui");

    GtkBuilder* builder = gtk_builder_new();
    if (gtk_builder_add_from_file(builder, uiFilePath, &error) == 0)
    {
        g_printerr("Error loading file: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    GObject* window = gtk_builder_get_object(builder, "window");

    gtk_window_fullscreen(GTK_WINDOW(window));

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GObject* button = gtk_builder_get_object(builder, "resetMinMax");
    g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);

    button = gtk_builder_get_object(builder, "brightnessDown");
    g_signal_connect(button, "clicked", G_CALLBACK(setBrightnessDown), NULL);

    button = gtk_builder_get_object(builder, "brightnessUp");
    g_signal_connect(button, "clicked", G_CALLBACK(setBrightnessUp), NULL);

    button = gtk_builder_get_object(builder, "turnOff");
    g_signal_connect(button, "clicked", G_CALLBACK(shutDown), NULL);

    gtk_main();

    return 0;
}