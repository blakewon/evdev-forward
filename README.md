# evdev-forward

Grab your keyboard and mouse at the evdev level and forward them somewhere
else: into a second X server, or into virtual input devices you can hand to
another seat, a nested X server, or a container. A hotkey toggles forwarding
on and off, so you can switch between your desktop and the target without
unplugging anything.

Built for **multiboxing on a single machine**: run a second game client in its
own X server (an openbox session on `:1`, say) and drive it with your real
keyboard and mouse.

- Single small binary, no daemon, no config file required.
- Keyboards and mice are detected automatically.
- Releasing and re-acquiring the devices cleans up held keys in both
  directions, so nothing is left stuck on either side of a toggle.

## How it works

On start, evdev-forward opens your keyboards and mice from `/dev/input`.
While forwarding is **on**, it takes an exclusive grab (`EVIOCGRAB`) so your
desktop stops seeing them, and sends every event to one of two backends:

| Backend | Selected by | Target |
|---|---|---|
| **uinput** (default) | no `--display` | one virtual device per source, named `evdev-forward <source name>`, with the source's capabilities and vendor/product ID |
| **XTest** | `--display=:N` | another X server, via the XTEST extension |

While forwarding is **off**, the devices are released and your desktop uses
them normally.

## Building

```sh
./configure
make
```

Requirements:

- Linux, a C++17 compiler, kernel headers
- **Optional, for the XTest backend:** libX11 and libXtst development files
  (Arch: `libxtst`, Debian/Ubuntu: `libxtst-dev`)

`./configure` detects XTest automatically. To control it explicitly:

```sh
./configure --with-xtest=yes   # fail if XTest isn't available
./configure --with-xtest=no    # build without any X dependency
```

`make clean` removes build output; `make distclean` also removes the
generated `config.mk` and `config.h`.

## Permissions

evdev-forward doesn't need root, but it needs:

1. **Read/write access to `/dev/input/event*`.** On most distributions these
   belong to the `input` group:

   ```sh
   sudo usermod -aG input $USER   # then log out and back in
   ```

2. **Access to `/dev/uinput`** (uinput backend only). No distribution grants
   this by default; it usually comes from packages like Steam or ydotool.
   To grant it yourself:

   ```
   # /etc/udev/rules.d/99-evdev-forward.rules
   KERNEL=="uinput", SUBSYSTEM=="misc", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"
   ```

   ```sh
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```

## Usage

```
evdev-forward [options] [device...]

  -d, --display=DISPLAY  forward into an X server via XTest (e.g. :1)
  -f, --devices=FILE     read device paths from FILE (one per line,
                         lines starting with # are comments)
  -n, --no-hotkey        forward immediately, no toggle
  -v, --verbose          print every forwarded event
  -h, --help             show help
```

With no devices given (on the command line or via `-f`), keyboards and mice
are detected automatically.

**Ctrl+Shift+Space** toggles forwarding. evdev-forward starts with forwarding
off.

### Examples

```sh
# auto-detect, uinput backend
evdev-forward

# forward into a second X server
evdev-forward --display=:1

# specific devices (by-id paths survive reboots and replugging)
evdev-forward /dev/input/by-id/usb-Logitech_USB_Receiver-event-mouse

# device list from a file
evdev-forward -f ~/.config/evdev-forward/devices
```

A device file:

```
# keyboard and mouse for the second client
/dev/input/by-id/usb-Keychron_K2-event-kbd
/dev/input/by-id/usb-Logitech_USB_Receiver-event-mouse
```

### `--no-hotkey`

Forwards from the moment it starts and never toggles. Ctrl+Shift+Space is
forwarded like any other key.

**There is no off switch.** If the target is another seat or `--display`, your
desktop has no keyboard or mouse until the process exits. It prints its PID
on start; stop it from another session with:

```sh
kill <pid>          # or: pkill evdev-forward
```

On SIGTERM it releases held keys, ungrabs the devices, and removes its
virtual devices before exiting.

## Using the virtual devices

The uinput backend creates one virtual device per source, named
`evdev-forward <source name>`. What you do with them is up to you. To list
them:

```sh
for f in /sys/class/input/event*; do
    printf '%s  %s\n' "${f##*/}" "$(cat $f/device/name)"
done | grep evdev-forward
```

### Assign them to another seat

Their sysfs paths change on every run, so match them by name:

```
# /etc/udev/rules.d/72-evdev-forward-seat.rules
SUBSYSTEM=="input", ATTRS{name}=="evdev-forward *", ENV{ID_SEAT}="seat1", TAG+="seat"
```

### Feed them to a nested X server

Xephyr can read evdev devices directly:

```sh
Xephyr :1 -keybd evdev,,device=/dev/input/eventN -mouse evdev,,device=/dev/input/eventM
```

### Pass them into a container

```sh
docker run --device /dev/input/eventN ...
```

The containerized application reads a copy of your hardware while your
desktop is unaffected, and only receives input while forwarding is on.

## Testing with Xephyr

You can try the XTest backend on one machine with a nested X server:

```sh
Xephyr :1 -screen 1280x720 -ac -resizeable -sw-cursor -no-host-grab &
DISPLAY=:1 openbox &
DISPLAY=:1 xterm &
evdev-forward --display=:1
```

- `-sw-cursor` makes Xephyr draw its own pointer; without it the forwarded
  pointer is invisible.
- `-no-host-grab` is required: Xephyr's own grab toggle is Ctrl+Shift, which
  collides with evdev-forward's hotkey and traps your pointer.
- `-ac` disables access control. Fine for a local test window, not for
  anything reachable from a network.

On a Wayland desktop, GTK and Qt applications ignore `DISPLAY` when
`WAYLAND_DISPLAY` is set. Launch them with:

```sh
env -u WAYLAND_DISPLAY DISPLAY=:1 firefox &
```

`DISPLAY=:1 xdotool type hello` is a quick way to check that the target X
server accepts synthetic input before involving evdev-forward.

## Notes

**Pointer speed on KDE.** KDE stores pointer settings per device. A virtual
device is a different device, so on your own desktop it starts with the
default (Adaptive) acceleration profile. If your mouse uses Flat, the
forwarded pointer will feel faster. Set the `evdev-forward …` device's profile
once in System Settings → Mouse & Touchpad, with forwarding on.

## Limitations

- The hotkey is fixed to Ctrl+Shift+Space for now.
- Absolute pointing devices (tablets, touchscreens, gamepad sticks) are not
  supported; their `EV_ABS` events are not forwarded.
- No hotplug: devices are found once at startup. An unplugged device is
  dropped cleanly, but is not picked up again when reconnected.

## Alternatives

- **[netevent](https://github.com/Blub/netevent)**: shares evdev devices over
  a network or locally, with scriptable hotkeys. Covers network forwarding and
  more device types; needs a setup script and runs a daemon plus a separate
  output process.
- **[evsieve](https://github.com/KarsMulder/evsieve)**: a general-purpose evdev
  remapper and router. Can route a grabbed device between virtual outputs with a hotkey; devices stay grabbed for the whole run.
- **QEMU `input-linux`**: the same grab toggle, for virtual machines.

None of these inject into a second X server, which is evdev-forward's main use.

## License

TBD
