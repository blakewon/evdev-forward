#include "devices/devices.h"
#include "forwarder/forwarder.h"

#include <cstdio>

#include <string.h>

int main(int argc, char **argv)
{
    Forwarder forwarder;
    forwarder.devices = collect_devices(argc, argv);

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
        {
            forwarder_set_verbose(true);
        }
    }

    if (forwarder.devices.count <= 0)
    {
        fprintf(stderr, "No input devices found (need root or the 'input' group)\n");
        return 1;
    }

    install_signal_handlers();

    if (!forwarder_setup(forwarder))
        return 1;

    forwarder_run(forwarder);
    return 0;
}

//TODO: EV_ABS for relative mice/touchpads/tablets
//TODO: Hotplug
//TODO: Accumulative motion