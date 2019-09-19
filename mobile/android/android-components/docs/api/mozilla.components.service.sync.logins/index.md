[android-components](../index.md) / [mozilla.components.service.sync.logins](./index.md)

## Package mozilla.components.service.sync.logins

### Types

| Name | Summary |
|---|---|
| [AsyncLoginsStorage](-async-logins-storage/index.md) | `interface AsyncLoginsStorage : `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html)<br>An interface equivalent to the LoginsStorage interface, but where operations are asynchronous. |
| [AsyncLoginsStorageAdapter](-async-logins-storage-adapter/index.md) | `open class AsyncLoginsStorageAdapter<T : LoginsStorage> : `[`AsyncLoginsStorage`](-async-logins-storage/index.md)`, `[`AutoCloseable`](https://developer.android.com/reference/java/lang/AutoCloseable.html)<br>A helper class to wrap a synchronous [LoginsStorage](#) implementation and make it asynchronous. |
| [SyncableLoginsStore](-syncable-logins-store/index.md) | `data class SyncableLoginsStore : `[`SyncableStore`](../mozilla.components.concept.sync/-syncable-store/index.md)<br>Wraps [AsyncLoginsStorage](-async-logins-storage/index.md) instance along with a lazy encryption key. |

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
| [SyncAuthInvalidException](-sync-auth-invalid-exception.md) | `typealias SyncAuthInvalidException = SyncAuthInvalidException`<br>This indicates that the authentication information (e.g. the [SyncUnlockInfo](-sync-unlock-info.md)) provided to [AsyncLoginsStorage.sync](-async-logins-storage/sync.md) is invalid. This often indicates that it's stale and should be refreshed with FxA (however, care should be taken not to get into a loop refreshing this information). |
| [SyncTelemetryPing](-sync-telemetry-ping.md) | `typealias SyncTelemetryPing = SyncTelemetryPing`<br>The telemetry ping from a successful sync |
| [SyncUnlockInfo](-sync-unlock-info.md) | `typealias SyncUnlockInfo = SyncUnlockInfo`<br>This type contains the set of information required to successfully connect to the server and sync. |

### Functions

| Name | Summary |
|---|---|
| [into](into.md) | `fun `[`SyncAuthInfo`](../mozilla.components.concept.sync/-sync-auth-info/index.md)`.into(): `[`SyncUnlockInfo`](-sync-unlock-info.md)<br>Conversion from a generic AuthInfo type into a type 'logins' lib uses at the interface boundary. |
