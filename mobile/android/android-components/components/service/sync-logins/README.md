# [Android Components](../../../README.md) > Service > Firefox Sync - Logins

A library for integrating with Firefox Sync - Logins.

## Motivation

The **Firefox Sync - Logins Component** provides a way for Android applications to do the following:

* Retrieve the Logins (url / password) data type from [Firefox Sync](https://www.mozilla.org/en-US/firefox/features/sync/)

## Usage

### Setting up the dependency

Use gradle to download the library from JCenter:

```
implementation "org.mozilla.components:sync-logins:{latest-version}
```

You will also need the Firefox Accounts component to be able to obtain the keys to decrypt the Logins data:

```
implementation "org.mozilla.components:fxa:{latest-version}
```

### Known Issues

* Android 6.0 is temporary not supported and will probably crash the application.

### Example

See [example service-sync-logins](../../../samples/sync-logins) for usage details.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
