[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FxaDeviceSettingsCache](./index.md)

# FxaDeviceSettingsCache

`class FxaDeviceSettingsCache : `[`SharedPreferencesCache`](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md)`<DeviceSettings>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaDeviceSettingsCache.kt#L27)

A thin wrapper around [SharedPreferences](#) which knows how to serialize/deserialize [DeviceSettings](#).

This class exists to provide background sync workers with access to [DeviceSettings](#).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaDeviceSettingsCache(context: <ERROR CLASS>)`<br>A thin wrapper around [SharedPreferences](#) which knows how to serialize/deserialize [DeviceSettings](#). |

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
| [fromJSON](from-j-s-o-n.md) | `fun fromJSON(obj: <ERROR CLASS>): DeviceSettings`<br>A conversion method from [JSONObject](#) to [T](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md#T). |
| [toJSON](to-j-s-o-n.md) | `fun DeviceSettings.toJSON(): <ERROR CLASS>`<br>A conversion method from [T](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md#T) into a [JSONObject](#). |
| [updateCachedName](update-cached-name.md) | `fun updateCachedName(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [clear](../../mozilla.components.support.base.utils/-shared-preferences-cache/clear.md) | `fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Clear cached values. |
| [getCached](../../mozilla.components.support.base.utils/-shared-preferences-cache/get-cached.md) | `fun getCached(): `[`T`](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md#T)`?` |
| [setToCache](../../mozilla.components.support.base.utils/-shared-preferences-cache/set-to-cache.md) | `fun setToCache(obj: `[`T`](../../mozilla.components.support.base.utils/-shared-preferences-cache/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
