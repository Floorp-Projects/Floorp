[android-components](../index.md) / [mozilla.components.service.sync.logins](index.md) / [InvalidKeyException](./-invalid-key-exception.md)

# InvalidKeyException

`typealias InvalidKeyException = InvalidKeyException` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L96)

This error is emitted in two cases:

1. An incorrect key is used to to open the login database
2. The file at the path specified is not a sqlite database.

SQLCipher does not give any way to distinguish between these two cases.

