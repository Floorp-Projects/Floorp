# [Android Components](../../README.md) > Samples > Browser

![](src/main/res/mipmap-xhdpi/ic_launcher.png)

A simple browser app that is composed from the browser components in this repository.

⚠️ **Note**: This sample application is only a very basic browser. For a full-featured reference browser implementation see the **[reference-browser repository](https://github.com/mozilla-mobile/reference-browser)**.

## Build variants

The browser app has two product flavor dimensions:

* **channel**: Using different release channels of GeckoView: _nightly_, _beta_, _production_. In most cases you want to use the _nightly_ flavor as this will support all of the latest functionality.

* **abi**: Using different GeckoView or ServoView builds based on the processor architecture: _arm_, _x86_, _aarch64_. Lookup the processor architecture that matches your device. For most Android devices that's _arm_ or _aarch64_. For running on an emulator pick _x86_ and a matching hardware-accelerated emulator image.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
