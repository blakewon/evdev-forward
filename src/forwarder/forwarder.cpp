#include "forwarder/forwarder.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "config.h"
#include "devices/devices.h"
#include "sink/sink.h"
#include "sink/xtest.h"

static std::atomic<bool> g_stop{false};
static std::atomic<bool> g_active{false};
static void on_signal(int) {g_stop = true;}
static bool g_verbose = false;

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


static void print_event(const Device &device, const input_event &event)
{
    printf("%-22s %-3s code=%-5u value=%-5d", device.name, type_name(event.type), event.code, event.value);

    if (event.type == EV_KEY)
        printf(" (%s)", key_action(event.value));

    printf("\n");
}

static void forward_event(Forwarder &forwarder, uint32_t index, const input_event &event)
{
    if (g_verbose)
        print_event(forwarder.devices.items[index], event);

#ifdef HAVE_XTEST
    if (forwarder.display)
    {
        xtest_write(forwarder.xtest, event);
        return;
    }
#endif

    sink_write(forwarder.sinks.items[index], event);
}

void forwarder_set_verbose(bool flag)
{
    g_verbose = flag;
}

void install_signal_handlers()
{
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
}

// cleanup
Forwarder::~Forwarder()
{
    set_grabbed(devices, false);
    sinks_close(sinks);

#ifdef HAVE_XTEST
    xtest_close(this->xtest);
#endif

    if (epoll_fd >= 0)
    {
        close(epoll_fd);
    }

    for (size_t i = 0; i < devices.count; i++)
    {
        Device &device = devices.items[i];
        if (device.fd >= 0)
        {
            close(device.fd);
        }
    }

    free(devices.items);
}

bool forwarder_setup(Forwarder& forwarder)
{
    forwarder.epoll_fd = epoll_create1(0);

    if (forwarder.epoll_fd < 0)
    {
        fprintf(stderr, "epoll_create1: %s\n", strerror(errno));
        return false;
    }

    for (size_t i = 0; i < forwarder.devices.count; i++)
    {
        Device &device = forwarder.devices.items[i];

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.u32 = (uint32_t)i;

        if (epoll_ctl(forwarder.epoll_fd, EPOLL_CTL_ADD, device.fd, &event))
        {
            fprintf(stderr, "epoll_ctl %s: %s\n", device.path, strerror(errno));
            return false;
        }
        printf("Using %s - %s\n", device.path, device.name);
    }

    if (forwarder.display)
    {
    #ifdef HAVE_XTEST
        if (!xtest_open(forwarder.xtest, forwarder.display))
            return false;
    #else
        fprintf(stderr, "--display needs XTest support; this build has none. Install dependencies and re-run ./configure\n");
        return false;
    #endif
    }
    else if (!sinks_open(forwarder.sinks, forwarder.devices))
    {
        fprintf(stderr, "Could not open the sinks, exiting...\n");
        return false;
    }

    if (forwarder.always_on)
    {
        if (!set_grabbed(forwarder.devices, true))
        {
            fprintf(stderr, "Could not grab devices, exiting...\n");
            return false;
        }
        g_active = true;
        printf("forwarding (no hotkey). stop with: kill %d\n\n", (int)getpid());
    }
    else
    {
        printf("ready - Ctrl+Shift+Space toggles forwarding. Ctrl-C to quit.\n\n");
    }
    return true;
}

static bool hotkey_feed(Hotkey &hk, const input_event event)
{
    if (event.type != EV_KEY)
        return false;

    switch(event.code)
    {
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            hk.ctrl = (event.value != 0);
            return false;
        
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            hk.shift = (event.value != 0);
            return false;
        
        case KEY_SPACE:
            return event.value == 1 && hk.ctrl && hk.shift;

        default:
            return false;
    }
}

static void toggle_forwarding(Forwarder &forwarder)
{
    if (g_active)
    {
        for (size_t i = 0; i < forwarder.sinks.count; i++)
        {
            sink_release_held(forwarder.sinks.items[i], forwarder.devices.items[i].fd);
        }
        set_grabbed(forwarder.devices, false);
        g_active = false;
        printf("Forwarding OFF.\n");
    #ifdef HAVE_XTEST
        xtest_release_all(forwarder.xtest);
    #endif
    }
    else
    {
        for (size_t i = 0; i < forwarder.devices.count; i++)
        {
            release_hotkey_buttons(forwarder.devices.items[i]);
        }

        if (!set_grabbed(forwarder.devices, true))
        {
            fprintf(stderr, "grab failed, staying disengaged\n");
            return;
        }
        g_active = true;
        printf("Forwarding ON.\n");
    }
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
            uint32_t index = ready[i].data.u32;
            Device &device = forwarder.devices.items[index];
            input_event buffer[64];

            ssize_t r = read(device.fd, buffer, sizeof(buffer));
            if (r < 0)
            {
                // EAGAIN just means "nothing there afterall"
                if (errno == EAGAIN)
                    continue;

                // guard against unplugged devices
                if (errno == ENODEV)
                {
                    fprintf(stderr, "%s disconnected.\n", device.path);
                    epoll_ctl(forwarder.epoll_fd, EPOLL_CTL_DEL, device.fd, nullptr);
                    close(device.fd);
                    device.fd = -1;

                    if (index < forwarder.sinks.count)
                    {
                        int &sink_fd = forwarder.sinks.items[index].fd;
                        if (sink_fd >= 0)
                        {
                            ioctl(sink_fd, UI_DEV_DESTROY);
                            close(sink_fd);
                            sink_fd = -1;
                        }
                    }
                    continue;
                }

                fprintf(stderr, "read %s: %s\n", device.path, strerror(errno));
                continue;
            }

            if (r == 0)
                continue;

            size_t count = r / sizeof(input_event);

            for (size_t k = 0; k < count; k++)
            {
                const input_event &event = buffer[k];

                if (!forwarder.always_on && hotkey_feed(forwarder.hotkey, event))
                {
                    toggle_forwarding(forwarder);
                    continue;
                }

                if (g_active)
                    forward_event(forwarder, index, event);
                
            }
        }
    }

    printf("\nReleasing grabs ...\n");
}
