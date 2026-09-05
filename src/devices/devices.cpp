#include "devices/devices.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <sys/ioctl.h>
#include <unistd.h>

//checks if device key map supports a capability, e.g. BTN_LEFT
static bool test_bit(const unsigned char *bits, int n)
{
    return bits[n >> 3] & (1u << (n & 7));
}

static bool looks_like_mouse(const unsigned char *keys,
                             const unsigned char *rel,
                             const unsigned char* abs)
{
    if (!test_bit(keys, BTN_LEFT))
        return false;

    return test_bit(rel, REL_X) || test_bit(rel, REL_Y) || test_bit(abs, ABS_X) || test_bit(abs, ABS_Y);
}

static bool looks_like_keyboard(const unsigned char *keys)
{
    return test_bit(keys, KEY_A) || test_bit(keys, KEY_ESC) || test_bit(keys, KEY_SPACE) || test_bit(keys, KEY_ENTER);
}

static std::vector<std::string> list_event_nodes()
{
    std::vector<std::string> names;
    DIR *directory = opendir("/dev/input");

    if (!directory)
        return names;

    while (struct dirent *directory_entry = readdir(directory))
    {
        if (strncmp(directory_entry->d_name, "event", 5) == 0)
        {
            names.push_back(std::string("/dev/input/") + directory_entry->d_name);
        }
    }

    closedir(directory);
    std::sort(names.begin(), names.end());

    return names;
}

Device open_device(const std::string &path)
{
    Device device{-1, path, "?"};
    device.fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);

    char name[256] = "?";
    ioctl(device.fd, EVIOCGNAME(sizeof(name)), name);
    device.name = name;
 
    return device;
}

std::vector<Device> detect_devices()
{
    std::vector<Device> found;

    for (const std::string &path : list_event_nodes())
    {
        int file_descriptor = open(path.c_str(), O_RDONLY | O_NONBLOCK);

        if (file_descriptor < 0)
            continue;

        char name[256] = "?";
        ioctl(file_descriptor, EVIOCGNAME(sizeof(name)), name);

        unsigned char keys[KEY_MAX / 8 + 1] = {};
        unsigned char rel[REL_MAX / 8 + 1] = {};
        unsigned char abs[ABS_MAX / 8 + 1] = {};

        ioctl(file_descriptor, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
        ioctl(file_descriptor, EVIOCGBIT(EV_REL, sizeof(rel)), rel);
        ioctl(file_descriptor, EVIOCGBIT(EV_ABS, sizeof(abs)), abs);

        bool is_keyboard = looks_like_keyboard(keys);
        bool is_mouse = looks_like_mouse(keys, rel, abs);

        if (is_keyboard || is_mouse)
        {
            found.push_back(
                Device{file_descriptor,
                path, 
                name, 
                false, 
                is_mouse, 
                is_keyboard});
        }
        else 
        {
            close(file_descriptor);
        }
    }

    return found;
}

bool set_grabbed(std::vector<Device> &devices, bool grab)
{
    std::vector<Device *> changed_devices;

    for (Device& device : devices)
    {
        //temporary safeguard so that i can CTRL + C the process
        if (device.is_keyboard)
            continue;

        if (grab)
        {
            if(ioctl(device.fd, EVIOCGRAB, 1) < 0)
            {
                fprintf(stderr, "grab %s: %s\n", device.path.c_str(), strerror(errno));
                for (Device *changed_device : changed_devices)
                {
                    // release previously grabbed devices
                    ioctl(changed_device->fd, EVIOCGRAB, 0);
                    changed_device->grabbed = false;
                }
                return false;
            }
        }
        else
        {
            ioctl(device.fd, EVIOCGRAB, 0);
        }

        device.grabbed = grab;
        changed_devices.push_back(&device);
    }
    
    return true;
}

std::vector<Device> collect_devices(int argc, char** argv)
{
    if (argc <= 1)
        return detect_devices();

    std::vector<Device> devices;
    for (int i = 1; i < argc; i++)
    {
        Device device = open_device(argv[i]);
        if (device.fd < 0)
        {
            fprintf(stderr, "open %s: %s", argv[i], strerror(errno));
        }
        else
        {
            devices.push_back(std::move(device));
        }
    }

    return devices;
}