[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](index.md) / [uninstallAddon](./uninstall-addon.md)

# uninstallAddon

`fun uninstallAddon(addon: `[`Addon`](../-addon/index.md)`, onSuccess: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L149)

Uninstalls the provided [Addon](../-addon/index.md).

### Parameters

`addon` - the addon to uninstall.

`onSuccess` - (optional) callback invoked if the addon was uninstalled successfully.

`onError` - (optional) callback invoked if there was an error uninstalling the addon.