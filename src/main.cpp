#include "devices/devices.h"
#include "forwarder/forwarder.h"

#include <cerrno>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstring>

#include <getopt.h>
#include <string.h>


struct Options 
{
    const char *display = nullptr;
    const char *device_file = nullptr;
    bool no_hotkey = false;
    bool verbose = false;
    char **paths = nullptr;
    int path_count = 0;
};

struct Paths
{
    char **items = nullptr;
    size_t count = 0;
    size_t cap = 0;
};

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
        "Usage: %s [options] [device...]\n"
        "\n"
        "Grab input devices and forward them to virtual devices named\n"
        "'evdev-forward <source name>', or into an X server wwith --display.\n"
        "With no devices given, keyboards and mice are detected automatically.\n"
        "Ctrl+Shift+Space toggles forwarding."
        "\n"
        "   -d, --display=DISPLAY   forward into an X server via XTest (e.g. :1)\n"
        "   -f  --devices=FILE      read device paths from FILE(one per line,\n"
        "                           lines starting with # are comments)\n"
        "   -n  --no-hotkey         forward immediately, no toggle\n"
        "   -v, --verbose           print every forwarded event\n"
        "   -h, --help              show this message\n",
    prog);
}

static bool parse_options(int argc, char **argv, Options &options)
{
    static const option long_options[] =
    {
        {"display", required_argument, nullptr, 'd'},
        {"devices", required_argument, nullptr, 'f'},
        {"no-hotkey", required_argument, nullptr, 'n'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    int c;
    while ((c = getopt_long(argc, argv, "d:s:vh", long_options, nullptr)) != -1)
    {
        switch (c)
        {
            case 'd': options.display = optarg; break;
            case 'f': options.device_file = optarg; break;
            case 'n': options.no_hotkey = true; break;
            case 'v': options.verbose = true; break;
            case 'h': usage(stdout, argv[0]); exit(0); break;
            default:
                fprintf(stderr, "Try '%s --help.\n", argv[0]);
                return false;
        }
    }

    options.paths = argv + optind;
    options.path_count = argc - optind;
    return true;
}

static bool paths_push(Paths &paths, const char *path)
{
    if (paths.count == paths.cap)
    {
        size_t cap = paths.cap ? paths.cap * 2 : 8;
        char **grown = (char **)realloc(paths.items, cap* sizeof(char *));
        if (grown == nullptr)
            return false;

        paths.items = grown;
        paths.cap = cap;
    }

    char *copy = strdup(path);
    if (copy == nullptr)
        return false;

    paths.items[paths.count++] = copy;
    return true;
}

static void paths_free(Paths &paths)
{
    for (size_t i = 0; i < paths.count; i++)
    {
        free(paths.items[i]);
    }

    free(paths.items);
    paths = Paths{};
}

static bool read_device_file(const char *file, Paths &paths)
{
    FILE *f = fopen(file, "r");
    if (f == nullptr)
    {
        fprintf(stderr, "%s: %s\n", file, strerror(errno));
        return false;
    }

    char *line = nullptr;
    size_t cap = 0;
    bool ok = true;

    while (getline(&line, &cap, f) != -1)
    {
        char *start = line;
        while (isspace((unsigned char)*start))
            start++;

        char *end = start + strlen(start);
        while (end > start && isspace((unsigned char)end[-1]))
        {
            end--;
        }
        *end = '\0';

        if (*start == '\0' || *start == '#')
            continue;

        if (!paths_push(paths, start))
        {
            fprintf(stderr, "%s: out of memory\n", file);
            ok = false;
            break;
        }
    }
    free(line);
    fclose(f);
    return ok;
}

static bool gather_paths(const Options &options, Paths &paths)
{
    for (int i = 0; i < options.path_count; i++)
    {
        if (!paths_push(paths, options.paths[i]))
        {
            fprintf(stderr, "out of memory\n");
            return false;
        }
    }

    if (options.device_file == nullptr)
        return true;

    size_t before = paths.count;

    if (!read_device_file(options.device_file, paths))
        return false;

    if (paths.count == before)
    {
        fprintf(stderr, "%s: no device paths in file\n", options.device_file);
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    Options options;
    if (!parse_options(argc, argv, options))
        return 2;

    Paths paths;
    if (!gather_paths(options, paths))
    {
        paths_free(paths);
        return 1;
    }

    Forwarder forwarder;
    forwarder.display = options.display;
    forwarder.always_on = options.no_hotkey;
    forwarder_set_verbose(options.verbose);

    forwarder.devices = collect_devices(paths.count, paths.items);
    paths_free(paths);

    if (forwarder.devices.count == 0)
    {
        fprintf(stderr, "No input devices found (need the 'input group or root)\n");
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
//TODO: Batch uinputs to an array per frame, cutting down on syscalls