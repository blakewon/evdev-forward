#include "devices/devices.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <linux/input.h>

int main(int argc, char **argv)
{
    std::vector<Device> devices;

    if (argc > 1)
    {
        for (int i = 1; i < argc; i++)
        {
            Device device = open_device(argv[i]);
            if (device.fd < 0)
            {
                fprintf(stderr, "open %s: %s\n", argv[1], strerror(errno));
                return 1;
            }

            devices.push_back(std::move(device));
        }
    } 
    else
    {
        devices = detect_devices();
    }

    if (devices.empty())
    {
        fprintf(stderr, "No input devices found. (need root or membership in the 'input' group)\n");
        return 1;
    }

    for (const Device &device: devices)
    {
        printf("Using %s - %s\n", device.path.c_str(), device.name.c_str());
    }
}