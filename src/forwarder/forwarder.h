#pragma once

#include "devices/devices.h"
#include "sink/sink.h"

struct Forwarder
{
    Devices devices;
    int epoll_fd = -1;
    Sink sink;

    ~Forwarder();

    Forwarder() = default;
    Forwarder(const Forwarder&) = delete;
    Forwarder& operator=(const Forwarder&) = delete;
};

void install_signal_handlers();
bool forwarder_setup(Forwarder& fwd);
void forwarder_run(Forwarder& fwd);