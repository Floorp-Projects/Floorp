[android-components](../../index.md) / [mozilla.components.support.base.utils](../index.md) / [SharedPreferencesCache](./index.md)

# SharedPreferencesCache

`abstract class SharedPreferencesCache<T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/utils/SharedPreferencesCache.kt#L16)

An abstract wrapper around [SharedPreferences](#) which facilitates caching of [T](index.md#T) objects.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SharedPreferencesCache(context: <ERROR CLASS>)`<br>An abstract wrapper around [SharedPreferences](#) which facilitates caching of [T](index.md#T) objects. |

### Properties

| Name | Summary |
|---|---|
| [cacheKey](cache-key.md) | `abstract val cacheKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Name of the 'key' under which serialized data is stored within the cache. |
| [cacheName](cache-name.md) | `abstract val cacheName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Name of the cache. |
| [context](context.md) | `val context: <ERROR CLASS>` |
| [logger](logger.md) | `abstract val logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md)<br>Logger used to report issues. |

### Functions

| Name | Summary |
|---|---|
| [clear](clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clear cached values. |
| [fromJSON](from-j-s-o-n.md) | `abstract fun fromJSON(obj: <ERROR CLASS>): `[`T`](index.md#T)<br>A conversion method from [JSONObject](#) to [T](index.md#T). |
| [getCached](get-cached.md) | `fun getCached(): `[`T`](index.md#T)`?` |
| [setToCache](set-to-cache.md) | `fun setToCache(obj: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [toJSON](to-j-s-o-n.md) | `abstract fun `[`T`](index.md#T)`.toJSON(): <ERROR CLASS>`<br>A conversion method from [T](index.md#T) into a [JSONObject](#). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [FxaDeviceSettingsCache](../../mozilla.components.service.fxa/-fxa-device-settings-cache/index.md) | `class FxaDeviceSettingsCache : `[`SharedPreferencesCache`](./index.md)`<DeviceSettings>`<br>A thin wrapper around [SharedPreferences](#) which knows how to serialize/deserialize [DeviceSettings](#). |
| [SyncAuthInfoCache](../../mozilla.components.service.fxa/-sync-auth-info-cache/index.md) | `class SyncAuthInfoCache : `[`SharedPreferencesCache`](./index.md)`<`[`SyncAuthInfo`](../../mozilla.components.concept.sync/-sync-auth-info/index.md)`>`<br>A thin wrapper around [SharedPreferences](#) which knows how to serialize/deserialize [SyncAuthInfo](../../mozilla.components.concept.sync/-sync-auth-info/index.md). |
