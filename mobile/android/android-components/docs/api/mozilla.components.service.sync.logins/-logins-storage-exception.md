[android-components](../index.md) / [mozilla.components.service.sync.logins](index.md) / [LoginsStorageException](./-logins-storage-exception.md)

# LoginsStorageException

`typealias LoginsStorageException = LoginsStorageException` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L47)

The base class of all errors emitted by logins storage.

Concrete instances of this class are thrown for operations which are
not expected to be handled in a meaningful way by the application.

For example, caught Rust panics, SQL errors, failure to generate secure
random numbers, etc. are all examples of things which will result in a
concrete `LoginsStorageException`.

