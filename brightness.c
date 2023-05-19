#include<fcntl.h> 

#define BRIGHTNESS_SYSTEM_PATH "/sys/class/backlight/10-0045/brightness"
#define BRIGHTNESS_INCREMENT 32

static void setBrightness(gboolean isUp)
{
    const int fileDesc = open(BRIGHTNESS_SYSTEM_PATH, O_RDWR);
    if (fileDesc < 0)
    {
        g_warning("Could not access " BRIGHTNESS_SYSTEM_PATH);
        return;
    }

    char brightnessString[10];
    const int readResult = read(fileDesc, brightnessString, sizeof(brightnessString));
    if (readResult < 0) {
        g_warning("Could not read " BRIGHTNESS_SYSTEM_PATH);
        close(fileDesc);
        return;
    }

    int brightness;
    sscanf(brightnessString, "%d", &brightness);

    brightness += isUp == TRUE ? BRIGHTNESS_INCREMENT : -BRIGHTNESS_INCREMENT;
    brightness = brightness > 255 ? 255 : brightness < 0 ? 0 : brightness;

    sprintf(brightnessString, "%d\n", brightness);

    const int writeResult = write(fileDesc, brightnessString, strlen(brightnessString));
    if (writeResult < 0) {
        g_warning("Could not write " BRIGHTNESS_SYSTEM_PATH);
    }

    close(fileDesc);
}

static void setBrightnessDown() { setBrightness(FALSE); }

static void setBrightnessUp() { setBrightness(TRUE); }
