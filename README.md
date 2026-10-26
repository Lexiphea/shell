# caelestia-shell

A self-maintained fork of [caelestia-shell](https://github.com/caelestia-dots/shell), the desktop shell from the
[Caelestia dotfiles](https://github.com/caelestia-dots/caelestia). All credit for the original shell goes to its
authors (see [Credits](#credits)). This fork carries my own changes and is not affiliated with or supported by
upstream, so please don't report issues from it there.

Built with [Quickshell](https://quickshell.outfoxxed.me) for [Hyprland](https://hypr.land).

## Dependencies

- [`caelestia-cli`](https://github.com/caelestia-dots/cli)
- [`quickshell-git`](https://git.outfoxxed.me/quickshell/quickshell) - the git version, not the latest tag
- `qt6-base` (6.9 or newer), `qt6-declarative`
- `qt6-imageformats` - the image cache is stored as WebP
- [`qt6-m3shapes-git`](https://github.com/soramanew/m3shapes)
- `ddcutil`, `brightnessctl`, `networkmanager`, `lm_sensors`, `power-profiles-daemon`
- `libpipewire`, `aubio`, `libcava`
- `swappy`, `fish`, `bash`
- Fonts: `ttf-material-symbols-variable`, `ttf-rubik-vf`, `ttf-cascadia-code-nerd`

Optional:

- [`soulver-cpp-git`](https://github.com/vicinaehq/soulver-cpp) and the Swift runtime (`swift-bin`) for the
  launcher calculator. Without them the shell runs normally, just without the calculator
- `systemd` 254 or newer. When the shell runs as a systemd service, apps it launches get their own scope, so
  restarting the shell doesn't kill them

Build:

- `cmake` 3.19 or newer, `ninja`, `pkgconf`
- A C++20 compiler (`gcc` or `clang`)
- `qt6-shadertools`
- `git` - the version is read from the latest tag. Without tags, pass `-DVERSION=<version> -DGIT_REVISION=<commit>`
- Development headers for `libpipewire`, `aubio` and `libcava` (included in the Arch packages)

## Building

Build in the checkout and run the shell straight from the build directory. There's no install step.

```sh
git clone git@github.com:Lexiphea/shell.git ~/caelestia-shell
cd ~/caelestia-shell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Quickshell loads the QML from the checkout
mkdir -p ~/.config/quickshell
ln -s ~/caelestia-shell ~/.config/quickshell/caelestia
```

Don't run `sudo cmake --install build` with this setup. It installs a second copy elsewhere on the system that the
shell never loads.

## Running

Run with the build directory on the import and library paths:

```sh
QML2_IMPORT_PATH=~/caelestia-shell/build/qml: CAELESTIA_LIB_DIR=~/caelestia-shell/build/lib caelestia shell -d
```

Or as a systemd user service, `~/.config/systemd/user/caelestia-shell.service`:

```ini
[Unit]
Description=Caelestia Shell
After=graphical-session.target
PartOf=graphical-session.target

[Service]
Environment=QML2_IMPORT_PATH=%h/caelestia-shell/build/qml:
Environment=CAELESTIA_LIB_DIR=%h/caelestia-shell/build/lib
Type=simple
ExecStartPre=-/usr/bin/qs -c caelestia kill
ExecStart=/usr/bin/caelestia shell
Restart=always
RestartSec=1

[Install]
WantedBy=graphical-session.target
```

```sh
systemctl --user daemon-reload
systemctl --user enable --now caelestia-shell
```

QML changes are picked up as soon as you save. After changing C++ or shaders, or pulling updates, rebuild and
restart:

```sh
cmake --build build
systemctl --user restart caelestia-shell
```

## Usage

Keybinds are Hyprland [global shortcuts](https://wiki.hypr.land/Configuring/Basics/Binds/#dbus-global-shortcuts).
IPC commands run through `caelestia shell ...`; list them with `caelestia shell -s`.

The dashboard profile picture is read from `~/.face`. Wallpapers are read from `~/Pictures/Wallpapers`, or from
`paths.wallpaperDir` in the config. Type `wallpaper` in the launcher to pick one, or run `caelestia wallpaper -f <path>`.

### Calculator

With `soulver-cpp-git` installed, the launcher doubles as a calculator powered by SoulverCore. It handles maths,
units, currencies, dates and times, e.g. `20% of 150`, `5 km in miles`, `10 usd in eur` or `time in tokyo`. Press
Enter to copy the result.

It only shows when no app or action matches, so it never takes Enter from an app. To force it, start the query
with `=`, e.g. `=pi`.

## Configuring

Options go in `~/.config/caelestia/shell.json`, which you create yourself. Only add the options you want to
change; anything omitted uses its default. Every option and its default is defined in
[`plugin/src/Caelestia/Config`](plugin/src/Caelestia/Config).

Per-monitor overrides go in `~/.config/caelestia/monitors/<monitor>/shell.json` (list monitor names with
`hyprctl monitors -j | jq -r '.[].name'`). Some global options, like animations and app commands, ignore
per-monitor overrides.

Internal tokens such as sizes, rounding, spacing and animation timings can be overridden in
`~/.config/caelestia/shell-tokens.json`. Change these with care: they can break the layout.

## Credits

This is a fork of [caelestia-shell](https://github.com/caelestia-dots/shell) by
[@soramanew](https://github.com/soramanew) and contributors. Upstream in turn thanks
[@outfoxxed](https://github.com/outfoxxed) for Quickshell, [@end-4](https://github.com/end-4) for
[dots-hyprland](https://github.com/end-4/dots-hyprland), and [Ax-Shell](https://github.com/Axenide/Ax-Shell) for
inspiration.

The calculator uses [SoulverCore](https://github.com/soulverteam/SoulverCore) through
[soulver-cpp](https://github.com/vicinaehq/soulver-cpp), and its behaviour follows
[vicinae](https://github.com/vicinaehq/vicinae).

Licensed under the [GPL-3.0](LICENSE), like upstream.
