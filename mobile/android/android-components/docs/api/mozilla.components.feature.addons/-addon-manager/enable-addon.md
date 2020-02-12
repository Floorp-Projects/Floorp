[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](index.md) / [enableAddon](./enable-addon.md)

# enableAddon

`fun enableAddon(addon: `[`Addon`](../-addon/index.md)`, onSuccess: (`[`Addon`](../-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L173)

Enables the provided [Addon](../-addon/index.md).

### Parameters

`addon` - the [Addon](../-addon/index.md) to enable.

`onSuccess` - (optional) callback invoked with the enabled [Addon](../-addon/index.md).

`onError` - (optional) callback invoked if there was an error enabling