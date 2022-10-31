# [Android Components](../../README.md) > Samples > Firefox Sync

![](src/main/res/mipmap-xhdpi/ic_launcher.png)

A simple app showcasing how to use `FxaAccountManager` together with `BackgroundSyncManager` and `browser-storage-sync` components
to (periodically) synchronize FxA data in a background worker.

## Concepts

This app demonstrates how to synchronize Firefox Account data (bookmarks, history, ...).

This app could be "easily" generalized to synchronize other types of data stores, if another implementation of `concept-storage`
is used.

Following basic bits of functionality are present:

* Configuring `FxaAccountManager` and `BackgroundSyncManager`
* Making `Syncable` stores (such as `PlacesHistoryStorage`) available to background workers via `GlobalSyncableStoreProvider`
* Configuring listeners to monitor account status (logged in, logged out) and sync status (in-progress, finished, querying local data)

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
