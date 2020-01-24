[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [UnsupportedAddonsAdapterDelegate](index.md) / [onUninstallError](./on-uninstall-error.md)

# onUninstallError

`open fun onUninstallError(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/UnsupportedAddonsAdapterDelegate.kt#L23)

Callback invoked if there was an error uninstalling the addon.

### Parameters

`addonId` - The unique id of the [Addon](#).

`throwable` - An exception to log.