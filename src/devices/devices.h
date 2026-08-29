#pragma once

#include <string>
#include <vector>

struct Device
{
    int fd;
    std::string path;
    std::string name;
};

std::vector<Device> detect_devices();
Device open_device(const std::string& path);