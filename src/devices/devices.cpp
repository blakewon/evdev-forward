#include "devices/devices.h"

#include <cstddef>
#include <cstring>
#include <cstdio>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>

//checks if device key map supports a capability, e.g. BTN_LEFT
static bool test_bit(const unsigned char *bits, int n)
{
    return bits[n >> 3] & (1u << (n & 7));
}

static bool looks_like_mouse(const unsigned char *keys, const unsigned char *rel, const unsigned char* abs)
{
    if (!test_bit(keys, BTN_LEFT))
        return false;

    return test_bit(rel, REL_X) || test_bit(rel, REL_Y) || test_bit(abs, ABS_X) || test_bit(abs, ABS_Y);
}

static bool looks_like_keyboard(const unsigned char *keys)
{
    return test_bit(keys, KEY_A) || test_bit(keys, KEY_ESC) || test_bit(keys, KEY_SPACE) || test_bit(keys, KEY_ENTER);
}


static int only_event_nodes(const struct dirent *entry)
{
    return strncmp(entry->d_name, "event", 5) == 0;
}

bool open_device(Device &out, const char *path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0)
    {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        return false;
    }

    if (snprintf(out.path, sizeof(out.path), "%s", path) >= (int)sizeof(out.path))
    {
        close(fd);
        fprintf(stderr, "path too long: %s\n", path);
        return false;
    }
    
    strcpy(out.name, "unknown");
    ioctl(fd, EVIOCGNAME(sizeof(out.name)), out.name);
    out.fd = fd;
    return true;
}

Devices detect_devices()
{
    struct dirent **namelist = nullptr;
    int n = scandir("/dev/input", &namelist, only_event_nodes, alphasort);

    if (n < 0)
    {
        fprintf(stderr, "scandir /dev/input: %s\n", strerror(errno));
        return Devices{};
    }

    Devices devices;
    devices.items = (Device *)calloc(n, sizeof(Device));

    if (n > 0 && devices.items == nullptr)
    {
        fprintf(stderr, "detect_devices: out of memory\n");
        for (int i = 0; i < n; i++)
        {
            free(namelist[i]);
        }
        free(namelist);
        return Devices{};
    }

    for (int i = 0; i < n; i++)
    {
        char path[DEVICE_PATH_MAX];
        snprintf(path, sizeof(path), "/dev/input/%s", namelist[i]->d_name);
        free(namelist[i]);

        int fd = open(path, O_RDONLY | O_NONBLOCK);

        if (fd < 0)
            continue;

        unsigned char key_bits[KEY_MAX / 8 + 1] = {};
        unsigned char rel_bits[REL_MAX / 8 + 1] = {};
        unsigned char abs_bits[ABS_MAX / 8 + 1] = {};

        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
        ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), rel_bits);
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);

        bool is_mouse = looks_like_mouse(key_bits, rel_bits, abs_bits);
        bool is_keyboard = looks_like_keyboard(key_bits);

        if(!is_keyboard && !is_mouse)
        {
            close(fd);
            continue;
        }

        Device &slot = devices.items[devices.count];
        slot.fd = fd;
        slot.is_mouse = is_mouse;
        slot.is_keyboard = is_keyboard;

        snprintf(slot.path, sizeof(slot.path), "%s", path);
        strcpy(slot.name, "unknown");
        ioctl(fd, EVIOCGNAME(sizeof(slot.name)), slot.name);
        devices.count++;
    }

    free(namelist);
    return devices;
}

bool set_grabbed(Devices &devices, bool grab)
{
    Device **flipped = (Device **)calloc(devices.count, sizeof(Device *));
    if (devices.count > 0 && flipped == nullptr)
    {
        fprintf(stderr, "set_grabbed: out of memory\n");
        return false;
    }

    size_t flipped_count = 0;

    for (size_t i = 0; i < devices.count; i++)
    {
        Device &device = devices.items[i];

        // temporary safeguard
        if (device.is_keyboard)
            continue;

        if (device.grabbed == grab)
            continue;

        if (ioctl(device.fd, EVIOCGRAB, grab ? true : false) < 0)
        {
            fprintf(stderr, "EVIOCGRAB %s: %s\n", device.path, strerror(errno));

            for (size_t j = 0; j < flipped_count; j++)
            {
                ioctl(flipped[j]->fd, EVIOCGRAB, grab ? false : true);
                flipped[j]->grabbed = !grab;
            }

            free(flipped);
            return false;
        }

        device.grabbed = grab;
        flipped[flipped_count++] = &device;
    }

    free(flipped);
    return true;
}

Devices collect_devices(int argc, char **argv)
{
    if (argc <= 1)
        return detect_devices();

    Devices devices;
    devices.items = (Device *)calloc(argc - 1, sizeof(Device));

    if (devices.items == nullptr)
        return Devices{};

    for (int i = 1; i < argc; i++)
    {
        if (open_device(devices.items[devices.count], argv[i]))
        {
            devices.count++;
        }
    }

    return devices;
}