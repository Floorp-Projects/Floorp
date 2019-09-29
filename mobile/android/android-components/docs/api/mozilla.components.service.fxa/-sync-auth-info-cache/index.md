[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [SyncAuthInfoCache](./index.md)

# SyncAuthInfoCache

`class SyncAuthInfoCache : `[`SharedPreferencesCache`](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md)`<`[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/SyncAuthInfoCache.kt#L26)

A thin wrapper around [SharedPreferences](#) which knows how to serialize/deserialize [SyncAuthInfo](../../mozilla.components.concept.sync/-sync-auth-info/index.md).

This class exists to provide background sync workers with access to [SyncAuthInfo](../../mozilla.components.concept.sync/-sync-auth-info/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncAuthInfoCache(context: <ERROR CLASS>)`<br>A thin wrapper around [SharedPreferences](#) which knows how to serialize/deserialize [SyncAuthInfo](../../mozilla.components.concept.sync/-sync-auth-info/index.md). |

### Properties

| Name | Summary |
|---|---|
| [cacheKey](cache-key.md) | `val cacheKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Name of the 'key' under which serialized data is stored within the cache. |
| [cacheName](cache-name.md) | `val cacheName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Name of the cache. |
| [logger](logger.md) | `val logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md)<br>Logger used to report issues. |

### Inherited Properties

| Name | Summary |
|---|---|
| [context](../../mozilla.components.support.base.utils/-shared-preferences-cache/context.md) | `val context: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [expired](expired.md) | `fun expired(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [fromJSON](from-j-s-o-n.md) | `fun fromJSON(obj: <ERROR CLASS>): `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)<br>A conversion method from [JSONObject](#) to [T](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md#T). |
| [toJSON](to-j-s-o-n.md) | `fun `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`.toJSON(): <ERROR CLASS>`<br>A conversion method from [T](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md#T) into a [JSONObject](#). |

### Inherited Functions

| Name | Summary |
|---|---|
| [clear](../../mozilla.components.support.base.utils/-shared-preferences-cache/clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clear cached values. |
| [getCached](../../mozilla.components.support.base.utils/-shared-preferences-cache/get-cached.md) | `fun getCached(): `[`T`](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md#T)`?` |
| [setToCache](../../mozilla.components.support.base.utils/-shared-preferences-cache/set-to-cache.md) | `fun setToCache(obj: `[`T`](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
