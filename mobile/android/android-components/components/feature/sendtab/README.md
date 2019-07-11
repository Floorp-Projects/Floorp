# [Android Components](../../../README.md) > Feature > SendTab

Feature component for sending tabs to other devices with a registered FxA Account.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-sendtab:{latest-version}"
```

## Usage

In order to make use of the send tab features here, it's required to have an an FxA Account setup.
See the [service-firefox-accounts](../../service/firefox-accounts/README.md) for more information how to set this up.

```kotlin

val sendTabUseCases = SendTabUseCases(accountManager)

// Send to a particular device
sendTabUseCases.sendToDeviceAsync("1234", TabData("Mozilla", "https://mozilla.org"))

// Send to all devices
sendTabUseCases.sendToAllAsync(TabData("Mozilla", "https://mozilla.org"))

// Send multiple tabs to devices works too..
sendTabUseCases.sendToDeviceAsync("1234", listof(tab1, tab2))
sendTabUseCases.sendToAllAsync(listof(tab1, tab2))

```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
