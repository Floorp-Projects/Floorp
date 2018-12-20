# [Android Components](../../../README.md) > Service > Firefox Sync - Logins

A library for integrating with Firefox Sync - Logins.

## Motivation

The **Firefox Sync - Logins Component** provides a way for Android applications to do the following:

* Retrieve the Logins (url / password) data type from [Firefox Sync](https://www.mozilla.org/en-US/firefox/features/sync/)

## Usage

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

### Example

How to display the data before syncing, sync, and then display the data after
syncing.

```kotlin
launch {
    AsyncLoginsStorageAdapter.forDatabase(dbPath).use { storage ->
        // This will throw if the database is corrupt or the key is incorrect.
        storage.unlock(mySecretKey).await()

        displayInUI(storage.list().await())

        // See getUnlockInfo in the sample for how to produce a SyncUnlockInfo
        // from an OAuthInfo provided by FxA.
        storage.sync(unlockInfo).await()

        displayInUI(storage.list().await())

        // Note: it's not necessary that you lock the DB before closing it,
        // it will happen automatically.
    }
}
```

See [example service-sync-logins](../../../samples/sync-logins) for a more in-depth usage example.

## API documentation

These types are all present under the `mozilla.components.service.sync.logins` namespace, however many are type aliases for types in `mozilla.appservices.logins`. Things not present in `mozilla.components.service.sync.logins` should be considered implementation details in most cases.

## `ServerPassword`

`ServerPassword` is a Kotlin [`data class`](https://kotlinlang.org/docs/reference/data-classes.html) which represents a login record that may either be stored locally or remotely. It contains the following fields:

#### `id: String`

The unique ID associated with this login. It is recommended that you not make assumptions about its format, as there are no restrictions imposed on it beyond uniqueness. In practice it is typically either 12 random Base64URL-safe characters or a UUID-v4 surrounded in curly-braces.

When inserting records (e.g. creating records for use with `AsyncLoginsStorage.add`), it is recommended that you leave it as the empty string, which will cause a unique id to be generated for you.

#### `hostname: String`

The hostname this record corresponds to. It is an error to attempt to insert or update a record to have a blank hostname, and attempting to do so `InvalidRecordException` being thrown.

#### `username: String? = null`

The username associated with this record, which may be blank if no username is asssociated with this login.

#### `password: String`

The password associated with this record. It is an error to insert or update a record to have a blank password, and attempting to do so `InvalidRecordException` being thrown.

#### `httpRealm: String? = null`

The challenge string for HTTP Basic authentication. Exactly one of `httpRealm` or `formSubmitURL` is allowed to be present, and attempting to insert or update a record to have both or neither will result in an `InvalidRecordException` being thrown.

#### `formSubmitURL: String? = null`

The submission URL for the form where this login may be entered. As mentioned above, exactly one of `httpRealm` or `formSubmitURL` is allowed to be present, and attempting to insert or update a record to have both or neither will result in an `InvalidRecordException` being thrown.

#### `timesUsed: Int = 0`

A lower bound on the number of times this record has been "used". This number may not record uses that occurred on remote devices (since they may not record the uses). This may be zero for records synced remotely that have no usage information.

A use is recorded (and `timeLastUsed` is updated accordingly) in the following scenarios:

- Newly inserted records have 1 use.
- Updating a record locally (that is, updates that occur from a sync do not count here) increments the use count.
- Calling `AsyncLoginsStorage.touch(id: String)`.

This is a metadata field, and as such, is ignored by `AsyncLoginsStorage.add` and `AsyncLoginsStorage.update`.

#### `timeCreated: Long = 0L`

An upper bound on the time of creation in milliseconds from the unix epoch. Not all clients record this so an upper bound is the best estimate possible.

This is a metadata field, and as such, is ignored by `AsyncLoginsStorage.add` and `AsyncLoginsStorage.update`.

#### `timeLastUsed: Long = 0L`

A lower bound on the time of last use in milliseconds from the unix epoch. This may be zero for records synced remotely that have no usage information. It is updated to the current timestamp in the same scenarios described in the documentation for `timesUsed`.

This is a metadata field, and as such, is ignored by `AsyncLoginsStorage.add` and `AsyncLoginsStorage.update`.

#### `timePasswordChanged: Long = 0L`

A lower bound on the time that the `password` field was last changed in milliseconds from the unix epoch. This is updated when a `AsyncLoginsStorage.update` operation changes the password of the record.

This is a metadata field, and as such, is ignored by `AsyncLoginsStorage.add` and `AsyncLoginsStorage.update`.

#### `usernameField: String? = null`

HTML field name of the username, if known.

#### `passwordField: String? = null`

HTML field name of the password, if known.

## FAQ

### Which exceptions do I need to handle?

It depends, but probably only `SyncAuthInvalidException`, but potentially `InvalidKeyException`.

- You need to handle `SyncAuthInvalidException`. You can do this by refreshing the FxA authentication (you should only do this once, and not in e.g. a loop). Most/All consumers will need to do this.

- `InvalidKeyException`: If you're sure the key you have used is valid, the only way to handle this is likely to delete the file containing the database (as the data is unreadable without the key). On the bright side, for sync users it should all be pulled down on the next sync.

- `MismatchedLockException`, `NoSuchRecordException`, `IdCollisionException`, `InvalidRecordException` all indicate problems with either your code or the arguments given to various functions. You may trigger and handle these if you like (it may be more convenient in some scenarios), but code that wishes to completely avoid them should be able to.

- `RequestFailedException`: This indicates a network error and it's probably safe to ignore this; or rather, you probably have some idea already about how you want to handle network errors.

The errors reported as "raw" `LoginsStorageException` are things like Rust panics, errors reported by OpenSSL or SQLcipher, corrupt data on the server (things that are not JSON after decryption), bugs in our code, etc. You don't need to handle these, and it would likely be beneficial (but of course not necessary) to report them via some sort of telemetry, if any is available.

### Can I use an in-memory SQLcipher connection with `DatabaseLoginsStorage`?

Yes, sort of. This works, however due to the fact that `lock` closes the database connection, *all data is lost when the database is locked*. This means that doing so will result in a database with different behavior around lock/unlock than one stored on disk.

That said, doing so is simple: Just create a `DatabaseLoginsStorage` with the path `:memory:`, and it will work. You may also use a [SQLite URI filename](https://www.sqlite.org/uri.html) with the parameter `mode=memory`. See https://www.sqlite.org/inmemorydb.html for more options and further information.

Note that we offer a `MemoryLoginsStorage` class which doesn't come with the same limitations (however it cannot sync).

### How do I set a key for the `DatabaseLoginsStorage`?

The key is automatically set the first time you unlock the database (this is due to the way `PRAGMA key`/`sqlite3_key` works).

Currently there is no way to change the key, once set (see https://github.com/mozilla/application-services/issues/256).

### Where is the source code for this?

Part is in this tree, but most is in https://github.com/mozilla/application-services. Specifically, there are two pieces not present in this repository, an [android-specific piece written in Kotlin](https://github.com/mozilla/application-services/tree/master/logins-sql/tree/master/logins-api/android), and a [cross-platform piece written in Rust](https://github.com/mozilla/application-services/tree/master/logins-sql).

### What's the difference between `wipe` and `wipeLocal`?

- `wipe` deletes all records, but remembers that it has deleted them. The next time we sync, we'll delete the remote versions of the deleted records too (e.g. if you wipe, then another client uploads a record, and then you sync, the other clients record will not be deleted).
- `wipeLocal` deletes all local data from the database, bringing us back to the state prior to the first local write (or sync). That is, it returns it to an empty database.
- Previous versions of this library offered a third variant, `reset`, which deletes last sync timestamps, and what the database believes is stored remotely. This is almost never what you want, and so it was removed, we mention it only to assist when updating this library.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
