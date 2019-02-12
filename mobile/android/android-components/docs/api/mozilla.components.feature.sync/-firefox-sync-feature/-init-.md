[android-components](../../index.md) / [mozilla.components.feature.sync](../index.md) / [FirefoxSyncFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FirefoxSyncFeature(syncableStores: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`SyncableStore`](../../mozilla.components.concept.storage/-syncable-store/index.md)`<`[`AuthType`](index.md#AuthType)`>>, reifyAuth: suspend (authInfo: `[`FxaAuthInfo`](../-fxa-auth-info/index.md)`) -> `[`AuthType`](index.md#AuthType)`)`

A feature implementation which orchestrates data synchronization of a set of [SyncableStore](../../mozilla.components.concept.storage/-syncable-store/index.md) which
all share a common [AuthType](index.md#AuthType).

[AuthType](index.md#AuthType) provides us with a layer of indirection that allows consumers of [FirefoxSyncFeature](index.md)
to use entirely different types of [SyncableStore](../../mozilla.components.concept.storage/-syncable-store/index.md), without this feature needing to depend on
their specific implementations. Those implementations might have heavy native dependencies
(e.g. places and logins depend on native libraries), and we do not want to force a consumer which
only cares about syncing logins to have to import a places native library.

### Parameters

`reifyAuth` - A conversion method which reifies a generic [FxaAuthInfo](../-fxa-auth-info/index.md) into an object of
type [AuthType](index.md#AuthType).