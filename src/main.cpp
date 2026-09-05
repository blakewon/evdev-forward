#include "devices/devices.h"
#include "forwarder/forwarder.h"

#include <cstdio>

int main(int argc, char **argv)
{
    Forwarder forwarder;
    forwarder.devices = collect_devices(argc, argv);

    if (forwarder.devices.empty())
    {
        fprintf(stderr, "No input devices found (need root or the 'input' group\n");
        return 1;
    }

    install_signal_handlers();

    if (!forwarder_setup(forwarder))
        return 1;

    forwarder_run(forwarder);
    return 0;
}