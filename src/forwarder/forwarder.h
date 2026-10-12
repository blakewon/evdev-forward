#pragma once

#include "devices/devices.h"
#include "sink/sink.h"
#include "sink/xtest.h"
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
    const char *display = nullptr;  //non-null means XTest mode
    XTestSink xtest;
    bool always_on = false;

    ~Forwarder();

    Forwarder() = default;
    Forwarder(const Forwarder&) = delete;
    Forwarder& operator=(const Forwarder&) = delete;
};

void install_signal_handlers();
bool forwarder_setup(Forwarder& fwd);
void forwarder_run(Forwarder& fwd);
void forwarder_set_verbose(bool flag);