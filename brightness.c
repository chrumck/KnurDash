#include<fcntl.h> 

#define BRIGHTNESS_SYSTEM_PATH "/sys/class/backlight/10-0045/brightness"
#define BRIGHTNESS_INCREMENT 32

static void setBrightness(gboolean isUp)
{
    const int fd = open(BRIGHTNESS_SYSTEM_PATH, O_RDWR);
    if (fd < 0)
    {
        g_warning("Could not access " BRIGHTNESS_SYSTEM_PATH);
        return;
    }

    char brString[10];
    int result = read(fd, brString, sizeof(brString));
    if (result < 0) {
        g_warning("Could not read " BRIGHTNESS_SYSTEM_PATH);
        close(fd);
        return;
    }

    int br;
    sscanf(brString, "%d", &br);

    br += isUp == TRUE ? BRIGHTNESS_INCREMENT : -BRIGHTNESS_INCREMENT;
    br = br > 255 ? 255 : br < 0 ? 0 : br;

    sprintf(brString, "%d\n", br);

    result = write(fd, brString, strlen(brString));
    if (result < 0) {
        g_warning("Could not write " BRIGHTNESS_SYSTEM_PATH);
    }

    close(fd);
}

static void setBrightnessDown() { setBrightness(FALSE); }
static void setBrightnessUp() { setBrightness(TRUE); }
