#pragma once

#include <linux/input.h>

#include "devices/devices.h"

struct Sink
{
    int fd = -1;
};
struct Sinks
{
    Sink *items = nullptr;
    size_t count = 0;
};

bool sinks_open(Sinks &sinks, const Devices &sources);
void sink_write(Sink& sink, const input_event &event);
void sinks_close(Sinks &sinks);