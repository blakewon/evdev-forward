#include "forwarder/forwarder.h"
#include "devices/devices.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <sys/epoll.h>
#include <unistd.h>

static std::atomic<bool> g_stop{false};
static void on_signal(int) {g_stop = true;}

void install_signal_handlers()
{
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
}

// print helper
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

// print helper
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

static void handle_event(const Device &device, const input_event &event)
{
    printf("%-22s %-3s code=%-5u value=%-5d", device.name.c_str(), type_name(event.type), event.code, event.value);

    if (event.type == EV_KEY)
    {
        printf(" (%s)", key_action(event.value));
    }

    printf("\n");
}

// cleanup
Forwarder::~Forwarder()
{
    set_grabbed(devices, false);

    if (epoll_fd >= 0)
    {
        close(epoll_fd);
    }

    for (const Device& device: devices)
    {
        if (device.fd >= 0)
        {
            close(device.fd);
        }
    }
}

bool forwarder_setup(Forwarder& forwarder)
{
    forwarder.epoll_fd = epoll_create1(0);

    if (forwarder.epoll_fd < 0)
    {
        fprintf(stderr, "epoll_create1: %s\n", strerror(errno));
        return false;
    }

    for (Device &device : forwarder.devices)
    {
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.ptr = &device;

        if (epoll_ctl(forwarder.epoll_fd, EPOLL_CTL_ADD, device.fd, &event))
        {
            fprintf(stderr, "epoll_ctl %s: %s\n", device.path.c_str(), strerror(errno));
            return false;
        }
        printf("Using %s - %s\n", device.path.c_str(), device.name.c_str());
    }

    if (!set_grabbed(forwarder.devices, true))
    {
        fprintf(stderr, "Could not grab all devices, exiting.\n");
        return false;
    }

    size_t grabbed = 0;

    for (const Device &device: forwarder.devices)
    {
        if(device.grabbed)
        {
            grabbed++;
        }
    }

    printf("\ngrabbed %zu device(s), the host no longer sees them. Ctrl-C to stop.\n\n", grabbed);
    return true;
}

void forwarder_run(Forwarder &forwarder)
{
    epoll_event ready[16];

    while (!g_stop)
    {
        int n = epoll_wait(forwarder.epoll_fd, ready, 16, -1);

        if (n < 0)
        {
            // EINTR means that a signal interrupted the wait
            if (errno == EINTR)
                continue;

            fprintf(stderr, "epoll_wait: %s\n", strerror(errno));
            break;
        }

        for (int i = 0; i < n; i++)
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

                fprintf(stderr, "read %s: %s\n", device.path.c_str(), strerror(errno));
                continue;
            }

            if (r == 0)
                continue;

            size_t count = r / sizeof(input_event);

            for (size_t k = 0; k < count; k++)
            {
                handle_event(device, buffer[k]);
            }
        }
    }

    printf("\nReleasing grabs ...\n");
}