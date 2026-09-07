#pragma once

#include <vector>

#include <linux/input.h>

#include "devices/devices.h"

struct Sink
{
    int fd = -1;

    ~Sink();

    Sink() = default;
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
};

bool sink_open(Sink &sink, const std::vector<Device> & sources);
void sink_write(Sink& sink, const input_event &event);