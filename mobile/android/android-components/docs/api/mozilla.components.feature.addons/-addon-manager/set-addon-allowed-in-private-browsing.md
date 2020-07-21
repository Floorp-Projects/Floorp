[android-components](../../index.md) / [mozilla.components.feature.addons](../index.md) / [AddonManager](index.md) / [setAddonAllowedInPrivateBrowsing](./set-addon-allowed-in-private-browsing.md)

# setAddonAllowedInPrivateBrowsing

`fun setAddonAllowedInPrivateBrowsing(addon: `[`Addon`](../-addon/index.md)`, allowed: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, onSuccess: (`[`Addon`](../-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }, onError: (`[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/AddonManager.kt#L255)

Sets whether to allow/disallow the provided [Addon](../-addon/index.md) in private browsing mode.

### Parameters

`addon` - the [Addon](../-addon/index.md) to allow/disallow.

`allowed` - true if allow, false otherwise.

`onSuccess` - (optional) callback invoked with the enabled [Addon](../-addon/index.md).

`onError` - (optional) callback invoked if there was an error enabling