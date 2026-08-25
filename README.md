# libjustcapture

A C library for XDG Desktop Portal integration on Linux mobile devices (Phosh/postmarketOS).

## Features

- **Portal connection management** — singleton session-bus connection with proxy caching
- **Screenshot Portal** — query targets and capture screenshots via XDG Desktop Portal
- **ScreenCast Portal** — full CreateSession → SelectSources → Start → OpenPipeWireRemote flow
- **Capability detection** — parallel probing of portal versions and available features
- **Output paths** — XDG user directories (Pictures/Screenshots, Videos/Screen Recordings)
- **Filename generation** — timestamped names with collision avoidance (_1, _2, ...)

## Requirements

- GLib ≥ 2.80, GIO
- Meson ≥ 1.3, Ninja
- pkg-config

## Build

```sh
meson setup builddir
ninja -C builddir
sudo ninja -C builddir install
```

## License

LGPL-2.1+
