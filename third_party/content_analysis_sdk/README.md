# Google Chrome Content Analysis Connector Agent SDK

The Google Chrome Content Analysis Connector provides an official mechanism
allowing Data Loss Prevention (DLP) agents to more deeply integrate their
services with Google Chrome.

DLP agents are background processes on managed computers that allow enterprises
to monitor locally running applications for data exfiltration events.  They can
allow/block these activities based on customer defined DLP policies.

This repository contains the SDK that DLP agents may use to become service
providers for the Google Chrome Content Analysis Connector.  The code that must
be compiled and linked into the content analysis agent is located in the `agent`
subdirectory.

A demo implementation of a service provider is located in the `demo` subdirectory.

The code that must be compiled and linked into Google Chrome is located in
the `browser` subdirectory.

The Protocol Buffer serialization format is used to serialize messages between the
browser and the agent. The protobuf definitions used can be found in the `proto`
subdirectory.

## Google Protocol Buffers

This SDK depends on Google Protocol Buffers version 3.18 or later.  It may be
installed from Google's [download page](https://developers.google.com/protocol-buffers/docs/downloads#release-packages)
for your developement platform.  It may also be installed using a package
manager.

The included prepare_build scripts use the Microsoft [vcpkg](https://github.com/microsoft/vcpkg)
package manager to install protobuf.  vcpkg is available on all supported
platforms.

## Build

### Pre-requisites

The following must be installed on the computer before building the demo:

- [git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git) version 2.33 or later.
- [cmake](https://cmake.org/install/) version 3.23 or later.
- A C++ compiler toolchain for your platform.
- On linux, the `realpath` shell tool, available in the `coreutils` package.
  In Debian-based distributions use `sudo apt intall coreutils`.
  In Red Hat distributions use `sudo yum install coreutils`.
- On Mac, use `brew install cmake coreutils pkg-config googletest` or an equivalent setup

### Running prepare_build

First get things ready by installing required dependencies:
```
$SDK_DIR/prepare_build <build-dir>
```
where `<build-dir>` is the path to a directory where the demo will be built.
By default, if no argument is provided, a directory named `build` will be
created in the project root. Any output within the `build/` directory will
be ignored by version control.

`prepare_build` performs the following steps:
1. Downloads the vcpkg package manager.
2. Downloads and builds the Google Protocol Buffers library.
3. Creates build files for your specific platform.

### Cmake Targets

To build the demo run the command `cmake --build <build-dir>`.

To build the protocol buffer targets run the command `cmake --build <build-dir> --target proto`
