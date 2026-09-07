#include "sink/sink.h"
#include "devices/devices.h"

#include <cstdio>
#include <cstring>
#include <cerrno>

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>


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

bool sink_open(Sink& sink, const std::vector<Device> &sources)
{
    sink.fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (sink.fd < 0)
    {
        fprintf(stderr, "open /dev/uinput: %s\n", strerror(errno));
        return false;
    }

    ioctl(sink.fd, UI_SET_EVBIT, EV_SYN);
    for (const Device &device: sources)
    {
        copy_capabilities(sink.fd, device.fd);
    }

    uinput_setup setup{};

    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1;
    setup.id.product = 0x1;
    strcpy(setup.name, "evdev-forward");

    if (ioctl(sink.fd, UI_DEV_SETUP, &setup) < 0)
    {
        fprintf(stderr, "UI_DEV_SETUP %s\n", strerror(errno));
        return false;
    }

    if (ioctl(sink.fd, UI_DEV_CREATE) < 0)
    {
        fprintf(stderr, "UI_DEV_CREATE: %s\n", strerror(errno));
        return false;
    }

    // let udev node be created before events flow, i should probably find a better way to do this
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

Sink::~Sink()
{
    if (fd >= 0)
    {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
    }
}

