#pragma once

#include "devices/devices.h"
#include "sink/sink.h"
struct Hotkey
{
    bool ctrl = false;
    bool shift = false;
};
struct Forwarder
{
    Devices devices;
    Sinks sinks;
    int epoll_fd = -1;
    Hotkey hotkey;

    ~Forwarder();

    Forwarder() = default;
    Forwarder(const Forwarder&) = delete;
    Forwarder& operator=(const Forwarder&) = delete;
};

void install_signal_handlers();
bool forwarder_setup(Forwarder& fwd);
void forwarder_run(Forwarder& fwd);