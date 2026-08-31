#include "devices/devices.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <set>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

static std::atomic<bool> g_stop{false};
static void on_signal(int) { g_stop = true; };

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

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        fprintf(stderr, "epoll_create1: %s\n", strerror(errno));
        return 1;
    }

    for (Device &device: devices)
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.ptr = &device;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, device.fd, &ev))
        {
            fprintf(stderr, "epoll_ctl %s: %s\n", device.path.c_str(), strerror(errno));
            return 1;
        }

        printf("Using %s - %s\n", device.path.c_str(), device.name.c_str());
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (!set_grabbed(devices, true))
    {
        fprintf(stderr, "could not grab all of the devices, exiting.\n");
        return 1;
    }

    size_t grabbed = 0;

    for (const Device& device : devices)
    {
        if (device.grabbed)
        {
            grabbed++;
        }
    }

    printf("\ngrabbed %zu devices(s), the host no longer sees them. Ctrl-C to stop.\n\n", grabbed);

    epoll_event ready[16];
    while (!g_stop)
    {
        int n = epoll_wait(epoll_fd, ready, 16, -1);

        if (n < 0)
        {
            // EINTR means that a signal interrupted the wait.
            if (errno == EINTR)
                continue;

            fprintf(stderr, "epoll_wait: %s\n", strerror(errno));
            break;
        }

        for(int i = 0; i < n; i++)
        {
            // reinterpret the devices we stored previously
            Device &device = *static_cast<Device *>(ready[i].data.ptr);

            input_event buffer[64];

            ssize_t r = read(device.fd, buffer, sizeof(buffer));

            if (r < 0)
            {
                // EAGAIN just means "nothing there afterall"
                if (errno == EAGAIN)
                    continue;
                
                fprintf(stderr, "Read %s: %s\n", device.path.c_str(), strerror(errno));
                continue;
            }

            if (r == 0)
                continue;

            // The kernel guarantees a fixed number of input events
            size_t count = r / sizeof(input_event);

            for (size_t k = 0; k < count; k++)
            {
                const input_event &event = buffer[k];
                printf("%-22s %-3s code=%-5u value=%-5d", device.name.c_str(), type_name(event.type), event.code, event.value);

                if (event.type == EV_KEY)
                {
                    printf(" (%s)", key_action(event.value));
                }

                printf("\n");
            }
        }
    }

    printf("\n Releasing grabs...\n");

    set_grabbed(devices, false);
    close (epoll_fd);
    for (const Device &device : devices)
    {
        close(device.fd);
    }

    return 0;
}