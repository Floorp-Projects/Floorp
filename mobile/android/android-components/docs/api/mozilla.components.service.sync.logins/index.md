[android-components](../index.md) / [mozilla.components.service.sync.logins](./index.md)

## Package mozilla.components.service.sync.logins

### Types

| Name | Summary |
|---|---|
| [DefaultLoginValidationDelegate](-default-login-validation-delegate/index.md) | `class DefaultLoginValidationDelegate : `[`LoginValidationDelegate`](../mozilla.components.concept.storage/-login-validation-delegate/index.md)<br>A delegate that will check against [storage](#) to see if a given Login can be persisted, and return information about why it can or cannot. |
| [GeckoLoginStorageDelegate](-gecko-login-storage-delegate/index.md) | `class GeckoLoginStorageDelegate : `[`LoginStorageDelegate`](../mozilla.components.concept.storage/-login-storage-delegate/index.md)<br>[LoginStorageDelegate](../mozilla.components.concept.storage/-login-storage-delegate/index.md) implementation. |
| [SyncableLoginsStorage](-syncable-logins-storage/index.md) | `class SyncableLoginsStorage : `[`LoginsStorage`](../mozilla.components.concept.storage/-logins-storage/index.md)`, `[`SyncableStore`](../mozilla.components.concept.sync/-syncable-store/index.md)`, `[`AutoCloseable`](http://docs.oracle.com/javase/7/docs/api/java/lang/AutoCloseable.html)<br>An implementation of [LoginsStorage](../mozilla.components.concept.storage/-logins-storage/index.md) backed by application-services' `logins` library. Synchronization support is provided both directly (via [sync](-syncable-logins-storage/sync.md)) when only syncing this storage layer, or via [getHandle](-syncable-logins-storage/get-handle.md) when syncing multiple stores. Use the latter in conjunction with [FxaAccountManager](#). |

### Type Aliases

| Name | Summary |
|---|---|
| [IdCollisionException](-id-collision-exception.md) | `typealias IdCollisionException = IdCollisionException`<br>This is thrown if `add()` is given a record whose `id` is not blank, and collides with a record already known to the storage instance. |
| [InvalidKeyException](-invalid-key-exception.md) | `typealias InvalidKeyException = InvalidKeyException`<br>This error is emitted in two cases: |
| [InvalidRecordException](-invalid-record-exception.md) | `typealias InvalidRecordException = InvalidRecordException`<br>This is thrown on attempts to insert or update a record so that it is no longer valid, where "invalid" is defined as such: |
| [LoginsStorageException](-logins-storage-exception.md) | `typealias LoginsStorageException = LoginsStorageException`<br>The base class of all errors emitted by logins storage. |
| [MismatchedLockException](-mismatched-lock-exception.md) | `typealias MismatchedLockException = MismatchedLockException`<br>This is thrown if `lock()`/`unlock()` pairs don't match up. |
| [NoSuchRecordException](-no-such-record-exception.md) | `typealias NoSuchRecordException = NoSuchRecordException`<br>This is thrown if `update()` is performed with a record whose ID does not exist. |
| [RequestFailedException](-request-failed-exception.md) | `typealias RequestFailedException = RequestFailedException`<br>This error is emitted if a request to a sync server failed. |
| [ServerPassword](-server-password.md) | `typealias ServerPassword = ServerPassword`<br>Raw password data that is stored by the storage implementation. |
| [SyncAuthInvalidException](-sync-auth-invalid-exception.md) | `typealias SyncAuthInvalidException = SyncAuthInvalidException`<br>This indicates that the authentication information (e.g. the [SyncUnlockInfo](-sync-unlock-info.md)) provided to [AsyncLoginsStorage.sync](#) is invalid. This often indicates that it's stale and should be refreshed with FxA (however, care should be taken not to get into a loop refreshing this information). |
| [SyncTelemetryPing](-sync-telemetry-ping.md) | `typealias SyncTelemetryPing = SyncTelemetryPing`<br>The telemetry ping from a successful sync |
| [SyncUnlockInfo](-sync-unlock-info.md) | `typealias SyncUnlockInfo = SyncUnlockInfo`<br>This type contains the set of information required to successfully connect to the server and sync. |

### Extensions for External Classes

| Name | Summary |
|---|---|
| [mozilla.appservices.logins.ServerPassword](mozilla.appservices.logins.-server-password/index.md) |  |

### Properties

| Name | Summary |
|---|---|
| [DB_NAME](-d-b_-n-a-m-e.md) | `const val DB_NAME: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [into](into.md) | `fun `[`SyncAuthInfo`](../mozilla.components.concept.sync/-sync-auth-info/index.md)`.into(): `[`SyncUnlockInfo`](-sync-unlock-info.md)<br>Conversion from a generic AuthInfo type into a type 'logins' lib uses at the interface boundary. |
| [toServerPassword](to-server-password.md) | `fun `[`Login`](../mozilla.components.concept.storage/-login/index.md)`.toServerPassword(): `[`ServerPassword`](-server-password.md)<br>Converts an Android Components [Login](../mozilla.components.concept.storage/-login/index.md) to an Application Services [ServerPassword](-server-password.md) |
