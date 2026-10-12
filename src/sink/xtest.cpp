#include "sink/xtest.h"

#include <X11/X.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <linux/input-event-codes.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

static constexpr unsigned int KEYCODE_OFFSET = 8;

static unsigned int button_for(unsigned short code)
{
    switch(code)
    {
        case BTN_LEFT:      return 1;
        case BTN_MIDDLE:    return 2;
        case BTN_RIGHT:     return 3;
        case BTN_SIDE:      return 8;
        case BTN_EXTRA:     return 9;
        default:            return 0;
    }
}

static void set_pressed(XTestSink &sink, unsigned short code, bool down)
{
    if (down)
        sink.pressed[code >> 3] |= (unsigned char)(1u << (code & 7));
    else
        sink.pressed[code >> 3] &= (unsigned char)~(1u << (code & 7));
}

static void click(XTestSink &sink, unsigned int button, int times)
{
    for (int i = 0; i < times; i++)
    {
        XTestFakeButtonEvent(sink.display, button, True , CurrentTime);
        XTestFakeButtonEvent(sink.display, button, False, CurrentTime);
    }
}

static int on_io_error(Display *)
{
    fprintf(stderr, "X connection lost, exiting\n");
    _exit(1);
}

bool xtest_open(XTestSink &sink, const char *name)
{
    sink.display = XOpenDisplay(name);
    if (sink.display == nullptr)
    {
        fprintf(stderr, "cannot open display %s\n", name);
        return false;
    }

    int event_base, error_base, major, minor;
    if (!XTestQueryExtension(sink.display, &event_base, &error_base, &major, &minor))
    {
        fprintf(stderr, "%s: server lacks the XTEST extension\n", name);
        XCloseDisplay(sink.display);
        sink.display = nullptr;
        return false;
    }

    XSetIOErrorHandler(on_io_error);
    return true;
}

void xtest_write(XTestSink &sink, const input_event &event)
{
    if (sink.display == nullptr)
        return;

    switch (event.type)
    {
        case EV_KEY:
        {
            if (event.value == 2)
                return;

            bool down = event.value != 0;

            if (unsigned int button = button_for(event.code))
            {
                XTestFakeButtonEvent(sink.display, button, down, CurrentTime);
            }
            else if (event.code + KEYCODE_OFFSET <= 255)
            {
                XTestFakeKeyEvent(sink.display, event.code + KEYCODE_OFFSET, down, CurrentTime);
            }
            else
                return;

            set_pressed(sink, event.code, down);
            return;
        }

        case EV_REL:
            switch(event.code)
            {
                case REL_X:         sink.dx += event.value; return;
                case REL_Y:         sink.dy += event.value; return;
                case REL_WHEEL:     click(sink, event.value > 0 ? 4 : 5, abs(event.value)); return;
                case REL_HWHEEL:    click(sink, event.value > 0 ? 7 : 6, abs(event.value)); return;
                default:            return;
            }
        
        case EV_SYN:
            if(event.code != SYN_REPORT)
                return;

            if (sink.dx != 0 || sink.dy != 0)
            {
                XTestFakeRelativeMotionEvent(sink.display, sink.dx, sink.dy, CurrentTime);
                sink.dx = 0;
                sink.dy = 0;
            }
            XFlush(sink.display);
            return;

        default:
            return;
    }
}

void xtest_release_all(XTestSink &sink)
{
    if(sink.display == nullptr)
        return;

    for (int code = 0; code <= KEY_MAX; code++)
    {
        if (!(sink.pressed[code >> 3] & (1u << (code & 7))))
            continue;

        if (unsigned int button = button_for(code))
        {
            XTestFakeButtonEvent(sink.display, button, False, CurrentTime);
        }
        else 
        {
            XTestFakeKeyEvent(sink.display, code + KEYCODE_OFFSET, False, CurrentTime);
        }
    }

    memset(sink.pressed, 0, sizeof(sink.pressed));
    sink.dx = 0;
    sink.dy = 0;
    XFlush(sink.display);
}

void xtest_close(XTestSink &sink)
{
    if (sink.display == nullptr)
        return;

    xtest_release_all(sink);
    XCloseDisplay(sink.display);
    sink.display = nullptr;
}