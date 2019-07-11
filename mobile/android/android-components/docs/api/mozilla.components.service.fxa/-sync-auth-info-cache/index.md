[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [SyncAuthInfoCache](./index.md)

# SyncAuthInfoCache

`class SyncAuthInfoCache` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/SyncAuthInfoCache.kt#L24)

A thin wrapper around [SharedPreferences](#) which knows how to serialize/deserialize [SyncAuthInfo](../../mozilla.components.concept.sync/-sync-auth-info/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncAuthInfoCache(context: <ERROR CLASS>)`<br>A thin wrapper around [SharedPreferences](#) which knows how to serialize/deserialize [SyncAuthInfo](../../mozilla.components.concept.sync/-sync-auth-info/index.md). |

### Properties

| Name | Summary |
|---|---|
| [context](context.md) | `val context: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [expired](expired.md) | `fun expired(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getCached](get-cached.md) | `fun getCached(): `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`?` |
| [setToCache](set-to-cache.md) | `fun setToCache(authInfo: `[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
