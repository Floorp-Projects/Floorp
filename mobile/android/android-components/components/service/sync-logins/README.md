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

## API documentation

These types are all present under the `org.mozilla.sync15.logins` namespace.

Anything present under `org.mozilla.sync15.logins.rust` should be considered private, and is only exposed due to implementation restrictions of JNA.

## `ServerPassword`

`ServerPassword` is a Kotlin [`data class`](https://kotlinlang.org/docs/reference/data-classes.html) which represents a login record that may either be stored locally or remotely. It contains the following fields:

#### `id: String`

The unique ID associated with this login. It is recommended that you not make assumptions about its format, as there are no restrictions imposed on it beyond uniqueness. In practice it is typically either 12 random Base64URL-safe characters or a UUID-v4 surrounded in curly-braces.

When inserting records (e.g. creating records for use with `LoginsStorage.add`), it is recommended that you leave it as the empty string, which will cause a unique id to be generated for you.

#### `hostname: String`

The hostname this record corresponds to. It is an error to attempt to insert or update a record to have a blank hostname, and attempting to do so `InvalidRecordException` being thrown.

#### `username: String?`

The username associated with this record, which may be blank if no username is asssociated with this login.

#### `password: String`

The password associated with this record. It is an error to insert or update a record to have a blank password, and attempting to do so `InvalidRecordException` being thrown.

#### `httpRealm: String? = null`

The challenge string for HTTP Basic authentication. Exactly one of `httpRealm` or `formSubmitURL` is allowed to be present, and attempting to insert or update a record to have both or neither will result in an `InvalidRecordException` being thrown.

#### `formSubmitURL: String? = null`

The submission URL for the form where this login may be entered. As mentioned above, exactly one of `httpRealm` or `formSubmitURL` is allowed to be present, and attempting to insert or update a record to have both or neither will result in an `InvalidRecordException` being thrown.

#### `timesUsed: Int`

A lower bound on the number of times this record has been "used". This number may not record uses that occurred on remote devices (since they may not record the uses). This may be zero for records synced remotely that have no usage information.

A use is recorded (and `timeLastUsed` is updated accordingly) in the following scenarios:

- Newly inserted records have 1 use.
- Updating a record locally (that is, updates that occur from a sync do not count here) increments the use count.
- Calling `LoginsStorage.touch(id: String)`.

This is a metadata field, and as such, is ignored by `LoginsStorage.add` and `LoginsStorage.update`.

#### `timeCreated: Long`

An upper bound on the time of creation in milliseconds from the unix epoch. Not all clients record this so an upper bound is the best estimate possible.

This is a metadata field, and as such, is ignored by `LoginsStorage.add` and `LoginsStorage.update`.

#### `timeLastUsed: Long`

A lower bound on the time of last use in milliseconds from the unix epoch. This may be zero for records synced remotely that have no usage information. It is updated to the current timestamp in the same scenarios described in the documentation for `timesUsed`.

This is a metadata field, and as such, is ignored by `LoginsStorage.add` and `LoginsStorage.update`.

#### `timePasswordChanged: Long`

A lower bound on the time that the `password` field was last changed in milliseconds from the unix epoch. This is updated when a `LoginsStorage.update` operation changes the password of the record.

This is a metadata field, and as such, is ignored by `LoginsStorage.add` and `LoginsStorage.update`.

#### `usernameField: String? = null`

HTML field name of the username, if known.

#### `passwordField: String? = null`

HTML field name of the password, if known.

## `LoginsStorage`

This is an interface describing the operations exposed by some underlying storage mechanism. Concrete implementors include `MemoryLoginsStorage` and `DatabaseLoginsStorage`.

#### `fun unlock(encryptionKey: String): SyncResult<Unit>`

This unlocks the `LoginsStorage` so that read/write operations may be performed on it.

Calling this when the storage is already unlocked will result in a `MismatchedLockException` being thrown.

#### `fun lock(): SyncResult<Unit>`

This locks the `LoginsStorage`, disposing of the database connection and sync state. After this, read/write operations may not be performed on it.

Calling this when the storage is already locked will result in a `MismatchedLockException` being thrown.

#### `fun isLocked(): SyncResult<Boolean>`

Resolves to true if the LoginsStorage is locked, and false otherwise.

#### `fun sync(syncInfo: SyncUnlockInfo): SyncResult<Unit>`

Attempt a sync with the remote server.

#### `fun reset(): SyncResult<Unit>`

Delete all locally stored sync metadata. It is unlikely that you should ever call this, and it may
be removed from a future version of the API.

#### `fun wipe(): SyncResult<Unit>`

Delete all locally stored records (replacing them with tombstones).

#### `fun delete(id: String): SyncResult<Boolean>`

Delete the record with the given `id`. Returns `false` if such no such record existed. For records which may have been synced, a tombstone is recorded so that the record may be deleted remotely.

#### `fun get(id: String): SyncResult<ServerPassword?>`

Get the record with the given `id`. Resoves to `null` if no record with that id exists.

#### `fun touch(id: String): SyncResult<Unit>`

Updates the `timesUsed` and `timeLastUsed` for the record with the given `id`. Throws a `NoSuchRecordException` if the ID doesn't refer to a known record.

#### `fun list(): SyncResult<List<ServerPassword>>`

Fetch the full list of passwords from the underlying storage layer.

#### `fun add(login: ServerPassword): SyncResult<String>`

Insert the provided login into the database.

This function ignores values in metadata fields (`timesUsed`, `timeCreated`, `timeLastUsed`, and `timePasswordChanged`).

If login has an empty id field, then a GUID will be generated automatically. The format of generated guids are left up to the implementation of LoginsStorage (in practice the `DatabaseLoginsStorage` generates 12-character [base64url](https://tools.ietf.org/html/rfc4648) encoded strings, and `MemoryLoginsStorage` generates strings using `java.util.UUID.toString`)

This will reject with `IdCollisionException` if a GUID is provided but collides with an existing record, or with `InvalidRecordException` if the provided record is invalid (see `InvalidRecordException` for more info).

#### `fun update(login: ServerPassword): SyncResult<Unit>`

Update the fields in the provided record.

This will reject with `NoSuchRecordException` if the `id` doesn't refer to a known record, or with `InvalidRecordException` if the provided record is invalid (see `InvalidRecordException` for more info).

This will reject  if `login.id` does not refer to a record that exists in the database, or if the provided record is invalid (missing password, hostname, or doesn't have exactly one of formSubmitURL and httpRealm).

Like `add`, this function ignores values in metadata fields (`timesUsed`, `timeCreated`, `timeLastUsed`, and `timePasswordChanged`).

### `DatabaseLoginStorage`

A concrete implementation of `LoginsStorage` which is backed by a SQLcipher database. It is initialized with the path to the database.

### `MemoryLoginsStorage`

A concrete implementation of `LoginsStorage` which is backed by an in-memory list. It is initialized with the list of initially present records.

### `SyncUnlockInfo`

This type contains the set of information required to successfully connect to the server and sync. See [the example application](../../../samples/sync-logins) for concrete usage information, including how to get one from the data provided by the FxA component.

## Exceptions

Several exception types may be thrown by various operations provided by this API. All of which are instances or subclasses of `LoginsStorageException`.

#### `LoginsStorageException`

Concrete instances of `LoginsStorageException` are thrown for operations which are not expected to be handled in a meaningful way by the application. For example, caught Rust panics, SQL errors, failure to generate secure random numbers, etc. are all examples of things which will result in a concrete `LoginsStorageException`.

#### `SyncAuthInvalidException`

This indicates that the authentication information (e.g. the `SyncUnlockInfo`) provided to `LoginsStorage.sync` is invalid. This often indicates that it's stale and should be refreshed with FxA (however, care should be taken not to get into a loop refreshing this information).

#### `MismatchedLockException`

This is thrown if the `lock()`/`unlock()` pairs in `LoginsStorage` usage do not match up. This is a bug in the code using `LoginsStorage`.

#### `NoSuchRecordException`

This is thrown if `update()` or `touch()` is called with a record id which does not exist.

#### `IdCollisionException`

This is thrown if `add()` is given a record whose `id` is not blank, and collides with a record already known to the `LoginsStorage` instance. You can avoid ever worrying about this error by always providing blank `id` when inserting new records.

#### `InvalidRecordException`

This error is thrown during `LoginsStorage.add` and `LoginsStorage.update` operations which would create or insert invalid records, where "invalid" is defined as such:

- A record with a blank `password` is invalid.
- A record with a blank `hostname` is invalid.
- A record that doesn't have a `formSubmitURL` nor a `httpRealm` is invalid.
- A record that has both a `formSubmitURL` and a `httpRealm` is invalid.

## `SyncResult`

This is a `Promise`/`Future`-like type based on `FxaResult`, which is used to represent asynchronous actions. More thorough usage examples are present in the documentation for the [example application](../../../samples/sync-logins).


## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
