# [Android Components](../../README.md) > Samples > Firefox Sync - History

![](src/main/res/mipmap-xhdpi/ic_launcher.png)

A simple app showcasing how to use `feature-sync` and `browser-storage-sync` components.

## Concepts

This app demonstrates how to synchronize browsing history using a Firefox Account.

Following basic bits of functionality are present:

* Configuring `feature-sync` with a custom CoroutineContext and a syncable history storage implementation
* Setting up a `FirefoxAccount` object with sync-friendly scopes, from a previous session or from scratch
* Using the authenticated account to synchronize history; querying history storage to confirm success

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
