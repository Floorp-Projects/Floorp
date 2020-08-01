[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [UnsupportedAddonsAdapterDelegate](./index.md)

# UnsupportedAddonsAdapterDelegate

`interface UnsupportedAddonsAdapterDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/UnsupportedAddonsAdapterDelegate.kt#L11)

Provides methods for handling the success and error callbacks from uninstalling an add-on in the
list of unsupported add-on items.

### Functions

| Name | Summary |
|---|---|
| [onUninstallError](on-uninstall-error.md) | `open fun onUninstallError(addonId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback invoked if there was an error uninstalling the addon. |
| [onUninstallSuccess](on-uninstall-success.md) | `open fun onUninstallSuccess(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Callback invoked if the addon was uninstalled successfully. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
