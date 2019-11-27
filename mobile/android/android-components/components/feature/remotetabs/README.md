# [Android Components](../../../README.md) > Feature > Remote Tabs

Feature component for viewing tabs from other devices with a registered Fx Account.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-remotetabs:{latest-version}"
```

## Usage

In order to make use of the remote tab feature here, it's required to have an an FxA Account setup and Sync enabled.
See the [service-firefox-accounts](../../service/firefox-accounts/README.md) for more information how to set this up.

```kotlin
  // The feature will start listening to local tabs changes.
  val remoteTabsFeature = RemoteTabsFeature(
    accountManager = accountManager,
    store = browserStore,
    tabsStorage = tabsStorage
  )
  // Grab the list of opened tabs on other devices.
  val otherDevicesTabs: Map<Device, List<Tab>> = remoteTabsFeature.getRemoteTabs()

```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
