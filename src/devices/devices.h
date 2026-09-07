#pragma once

#include <cstddef>

#include <limits.h>
#include <linux/limits.h>

constexpr size_t DEVICE_PATH_MAX = sizeof("/dev/input/") + NAME_MAX;

struct Device
{
    int fd = -1;
    char path[DEVICE_PATH_MAX] = {};
    char name[256] = {};
    bool grabbed = false;
    bool is_mouse = false;
    bool is_keyboard = false;
};

struct Devices
{
    Device *items = nullptr;
    size_t count = 0;
};

bool open_device(Device &out, const char *path);
Devices detect_devices();
Devices collect_devices(int argc, char **argv);
bool set_grabbed(Devices &devices, bool grab);