# [Android Components](../../../README.md) > Libraries > Nearby

A component simplifying use of Google's
[Nearby Connections API](https://developers.google.com/nearby/connections/overview),
which provides "peer-to-peer networking API that allows apps to easily
discover, connect to, and exchange data with nearby devices in
real-time, regardless of network connectivity. "

Main features:

* Support for communication via wifi or Bluetooth directly between two
  Android devices.
* Messages are securely encrypted (unless the client disables
  authentication, which is not recommended).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:lib-nearby:{latest-version}"
```

### Limitations

* Communication is limited to a pair of devices.
* Currently, messages are limited to 32 KB.
* To make a connection, one device must call `startAdvertising()` and
another must call `startDiscovering()`.
* This [depends on Google Play services 11.0 or higher](https://android-developers.googleblog.com/2017/07/announcing-nearby-connections-20-fully.html).

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
