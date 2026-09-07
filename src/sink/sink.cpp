#include "sink/sink.h"
#include "devices/devices.h"

#include <cstdio>
#include <cstring>
#include <cerrno>

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>


struct Group
{
    int event_type;
    int uinput_bit;
    int code_max;
};

static void copy_capabilities(int uinput_fd, int source_fd)
{
    Group groups[] = 
    {
        {EV_KEY, UI_SET_KEYBIT, KEY_MAX},
        {EV_REL, UI_SET_RELBIT, REL_MAX},
        {EV_MSC, UI_SET_MSCBIT, MSC_MAX}
    };

    for (const Group &group : groups)
    {
        unsigned char mask[KEY_MAX / 8 + 1] = {};
        if (ioctl(source_fd, EVIOCGBIT(group.event_type, sizeof(mask)), mask) < 0)
            continue;

        bool any = false;
        for (int code = 0; code <= group.code_max; code++)
        {
            if (mask[code >> 3] & (1u << (code & 7)))
            {
                ioctl(uinput_fd, group.uinput_bit, code);
                any = true;
            }
        }

        if (any)
        {
            ioctl(uinput_fd, UI_SET_EVBIT, group.event_type);
        }
    }
}

bool sink_open_one(Sink& sink, const Device &source)
{
    sink.fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (sink.fd < 0)
    {
        fprintf(stderr, "open /dev/uinput: %s\n", strerror(errno));
        return false;
    }

    ioctl(sink.fd, UI_SET_EVBIT, EV_SYN);
    copy_capabilities(sink.fd, source.fd);

    uinput_setup setup{};
    ioctl(source.fd, EVIOCGID, &setup.id);

    if (snprintf(setup.name, sizeof(setup.name), "evdev-forward %s", source.name) < 0)
    {
        strcpy(setup.name, "evdev-forward");
    }

    if (ioctl(sink.fd, UI_DEV_SETUP, &setup) < 0)
    {
        fprintf(stderr, "UI_DEV_SETUP %s\n", strerror(errno));
        return false;
    }

    if (ioctl(sink.fd, UI_DEV_CREATE) < 0)
    {
        fprintf(stderr, "UI_DEV_CREATE: %s\n", strerror(errno));
        close(sink.fd);
        sink.fd = -1;
        return false;
    }

    return true;
}

bool sinks_open(Sinks &sinks, const Devices &sources)
{
    sinks.items = (Sink *)calloc(sources.count, sizeof(Sink));

    if (sources.count > 0 && sinks.items == nullptr)
    {
        fprintf(stderr, "sinks_open: out of memory\n");
        return false;
    }

    for (size_t i = 0; i < sources.count; i++)
    {
        sinks.items[i].fd = -1;
        if (!sink_open_one(sinks.items[i], sources.items[i]))
        {
            return false;
        }
        sinks.count = i + 1;
    }

    // wait for nodes creation, i should probably find a better way to do this
    usleep(200 * 1000);
    return true;
}

void sink_write(Sink &sink, const input_event &event)
{
    if (write(sink.fd, &event, sizeof(event)) != (ssize_t)sizeof(event))
    {
        fprintf(stderr, "sink_write %s\n", strerror(errno));
    }
}

void sinks_close(Sinks &sinks)
{
    for (size_t i = 0; i < sinks.count; i++)
    {
        if (sinks.items[i].fd >= 0)
        {
            ioctl(sinks.items[i].fd, UI_DEV_DESTROY);
            close(sinks.items[i].fd);
        }
    }
}
