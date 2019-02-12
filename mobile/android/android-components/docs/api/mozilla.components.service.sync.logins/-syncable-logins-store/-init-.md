[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [SyncableLoginsStore](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SyncableLoginsStore(store: `[`AsyncLoginsStorage`](../-async-logins-storage/index.md)`, key: () -> Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>)`

Wraps [AsyncLoginsStorage](../-async-logins-storage/index.md) instance along with a lazy encryption key.

This helper class lives here and not alongside [AsyncLoginsStorage](../-async-logins-storage/index.md) because we don't want to
force a `service-sync-logins` dependency (which has a heavy native library dependency) on
consumers of [FirefoxSyncFeature](#).

