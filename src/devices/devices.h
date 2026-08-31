#pragma once

#include <string>
#include <vector>

struct Device
{
    int fd = -1;
    std::string path;
    std::string name;
    bool grabbed = false;
    bool is_mouse = false;
    bool is_keyboard = false;
};

std::vector<Device> detect_devices();
Device open_device(const std::string& path);
bool set_grabbed(std::vector<Device> &devices, bool grab);