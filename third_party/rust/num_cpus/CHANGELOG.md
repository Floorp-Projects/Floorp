## v1.7.0

#### Features

- add `get_physical` support for macOS

#### Fixes

- use `_SC_NPROCESSORS_CONF` on Unix targets

### v1.6.2

#### Fixes

- revert 1.6.1 for now

### v1.6.1

#### Fixes

- fixes sometimes incorrect num on Android/ARM Linux (#45)

## v1.6.0

#### Features

- `get_physical` gains Windows support

### v1.5.1

#### Fixes

- fix `get` to return 1 if `sysconf(_SC_NPROCESSORS_ONLN)` failed

## v1.5.0

#### Features

- `get()` now checks `sched_affinity` on Linux

## v1.4.0

#### Features

- add `haiku` target support

## v1.3.0

#### Features

- add `redox` target support

### v1.2.1

#### Fixes

- fixes `get_physical` count (454ff1b)

## v1.2.0

#### Features

- add `emscripten` target support
- add `fuchsia` target support

## v1.1.0

#### Features

- added `get_physical` function to return number of physical CPUs found

# v1.0.0

#### Features

- `get` function returns number of CPUs (physical and virtual) of current platform
