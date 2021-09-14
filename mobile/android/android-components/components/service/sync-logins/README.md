# [Android Components](../../../README.md) > Service > Firefox Sync - Logins

A library for integrating with Firefox Sync - Logins.

## Before using this component
Products sending telemetry and using this component *must request* a data-review following [this process](https://wiki.mozilla.org/Firefox/Data_Collection).
This component provides data collection using the [Glean SDK](https://mozilla.github.io/glean/book/index.html).
The list of metrics being collected is available in the [metrics documentation](../../support/sync-telemetry/docs/metrics.md).

## Motivation

The **Firefox Sync - Logins Component** provides a way for Android applications to do the following:

* Retrieve the Logins (url / password) data type from [Firefox Sync](https://www.mozilla.org/en-US/firefox/features/sync/)

## Usage

### Before using this component

The `mozilla.appservices.logins` component collects telemetry using the [Glean SDK](https://mozilla.github.io/glean/).
Applications that send telemetry via Glean *must ensure* they have received appropriate data-review following
[the Firefox Data Collection process](https://wiki.mozilla.org/Firefox/Data_Collection) before integrating this component.

Details on the metrics collected by the `mozilla.appservices.logins` component are available
[here](https://github.com/mozilla/application-services/tree/main/docs/metrics/logins/metrics.md)


### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```
implementation "org.mozilla.components:service-sync-logins:{latest-version}"
```

You will also need the Firefox Accounts component to be able to obtain the keys to decrypt the Logins data:

```
implementation "org.mozilla.components:fxa:{latest-version}
```

### Known Issues

* Android 6.0 is temporary not supported and will probably crash the application.

## API

This implements the login-related interfaces from `mozilla.components.concept.storage`.

## FAQ

### Which exceptions do I need to handle?

It depends, but probably only `SyncAuthInvalidException`, but potentially `CryptoError`.

- You need to handle `SyncAuthInvalidException`. You can do this by refreshing the FxA authentication (you should only do this once, and not in e.g. a loop). Most/All consumers will need to do this.

- `CryptoError`: If you're sure the key you have used is valid, the only way to handle this is likely to delete the file containing the database (as the data is unreadable without the key). On the bright side, for sync users it should all be pulled down on the next sync.

- `MismatchedLockException`, `NoSuchRecordException`, `InvalidRecordException` all indicate problems with either your code or the arguments given to various functions. You may trigger and handle these if you like (it may be more convenient in some scenarios), but code that wishes to completely avoid them should be able to.

- `RequestFailedException`: This indicates a network error and it's probably safe to ignore this; or rather, you probably have some idea already about how you want to handle network errors.

The errors reported as "raw" `LoginsStorageException` are things like Rust panics, errors reported by OpenSSL or SQLcipher, corrupt data on the server (things that are not JSON after decryption), bugs in our code, etc. You don't need to handle these, and it would likely be beneficial (but of course not necessary) to report them via some sort of telemetry, if any is available.

### Can I use an in-memory SQLcipher connection with `DatabaseLoginsStorage`?

Just create a `DatabaseLoginsStorage` with the path `:memory:`, and it will work. You may also use a [SQLite URI filename](https://www.sqlite.org/uri.html) with the parameter `mode=memory`. See https://www.sqlite.org/inmemorydb.html for more options and further information.

### What's the difference between `wipe` and `wipeLocal`?

- `wipe` deletes all records, but remembers that it has deleted them. The next time we sync, we'll delete the remote versions of the deleted records too (e.g. if you wipe, then another client uploads a record, and then you sync, the other clients record will not be deleted).
- `wipeLocal` deletes all local data from the database, bringing us back to the state prior to the first local write (or sync). That is, it returns it to an empty database.
- Previous versions of this library offered a third variant, `reset`, which deletes last sync timestamps, and what the database believes is stored remotely. This is almost never what you want, and so it was removed, we mention it only to assist when updating this library.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
