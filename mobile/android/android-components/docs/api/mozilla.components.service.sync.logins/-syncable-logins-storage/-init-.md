[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStorage](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SyncableLoginsStorage(context: <ERROR CLASS>, key: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`

An implementation of [LoginsStorage](../../mozilla.components.concept.storage/-logins-storage/index.md) backed by application-services' `logins` library.
Synchronization support is provided both directly (via [sync](sync.md)) when only syncing this storage layer,
or via [getHandle](get-handle.md) when syncing multiple stores. Use the latter in conjunction with [FxaAccountManager](#).

