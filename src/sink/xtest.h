#pragma once

#include <linux/input.h>

struct _XDisplay;       //forward-declared Xlib's display

struct XTestSink
{
    _XDisplay *display = nullptr;
    int dx = 0;
    int dy = 0;

    unsigned char pressed[KEY_MAX / 8 + 1] = {};
};

bool xtest_open(XTestSink &sink, const char *name);
void xtest_write(XTestSink &sink, const input_event &event);
void xtest_release_all(XTestSink &sink);
void xtest_close(XTestSink &sink);