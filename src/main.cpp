#include "devices/devices.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>


//temporary print helper
static const char *type_name(unsigned short type)
{
    switch(type)
    {
        case EV_SYN: return "SYN";
        case EV_KEY: return "KEY";
        case EV_REL: return "REL";
        case EV_ABS: return "ABS";
        case EV_MSC: return "MSC";
        default:     return "?";
    }
}

//temporary print helper
static const char *key_action(int value)
{
    switch (value)
    {
        case 0: return "release";
        case 1: return "press";
        case 2: return "repeat";
        default:return "?";
    }
}

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

    Device device = devices.at(0);
    printf("\nreading %s - %s\n\n", device.path.c_str(), device.name.c_str());

    while (true)
    {

        input_event buffer[64];

        ssize_t n = read(device.fd, buffer, sizeof(buffer));

        /*
            n < 0   error, reason is inside errno
            n == 0  end of file, for a device node it was probably unplugged
            n > 0   got n bytes, can be fewer than count, which is normal
        */
        if (n < 0)
        {
            fprintf(stderr, "Read %s: %s\n", device.path.c_str(), strerror(errno));
            break;
        }

        if (n == 0)
        {
            fprintf(stderr, "%s closed. (unplugged?)\n", device.path.c_str());
            break;
        }
        
        //The kernel guarantees a read() on an event* file descriptor returns a whole number of input_event structs
        size_t count = n / sizeof(input_event);

        for (size_t i = 0; i < count; i++)
        {
            const input_event &event = buffer[i];
            /*
                EV_SYN = 0  batch separator(next concept)
                EV_KEY = 1  a key button changed state
                EV_REL = 2  relative motion, e.g. mouse moved by an amount
                EV_ABS = 3  absolute positiion, pointer at a coordinate
                EV_MSC = 4  miscellaneous hardware info
            */
            printf("%-3s code=%-5u value=%-5d", type_name(event.type), event.code, event.value);
            
            if (event.type == EV_KEY)
            {
                printf(" (%s)", key_action(event.value));
            }
            
            printf("\n");
        }
    }
}