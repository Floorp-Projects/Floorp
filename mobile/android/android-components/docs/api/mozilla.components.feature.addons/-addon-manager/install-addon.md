[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](index.md) / [installAddon](./install-addon.md)

# installAddon

`fun installAddon(addon: `[`Addon`](../-addon/index.md)`, onSuccess: (`[`Addon`](../-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { _, _ -> }): `[`CancellableOperation`](../../mozilla.components.concept.engine/-cancellable-operation/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L110)

Installs the provided [Addon](../-addon/index.md).

### Parameters

`addon` - the addon to install.

`onSuccess` - (optional) callback invoked if the addon was installed successfully,
providing access to the [Addon](../-addon/index.md) object.

`onError` - (optional) callback invoked if there was an error installing the addon.