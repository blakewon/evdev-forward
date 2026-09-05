#pragma once

#include <vector>

#include "devices/devices.h"

struct Forwarder
{
    std::vector<Device> devices;
    int epoll_fd = -1;

    ~Forwarder();

    Forwarder() = default;
    Forwarder(const Forwarder&) = delete;
    Forwarder& operator=(const Forwarder&) = delete;
};

void install_signal_handlers();
bool forwarder_setup(Forwarder& fwd);
void forwarder_run(Forwarder& fwd);