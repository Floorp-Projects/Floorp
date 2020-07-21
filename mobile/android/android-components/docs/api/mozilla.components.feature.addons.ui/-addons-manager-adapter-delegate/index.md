[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [AddonsManagerAdapterDelegate](./index.md)

# AddonsManagerAdapterDelegate

`interface AddonsManagerAdapterDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/AddonsManagerAdapterDelegate.kt#L12)

Provides methods for handling the add-on items in the add-on manager.

### Functions

| Name | Summary |
|---|---|
| [onAddonItemClicked](on-addon-item-clicked.md) | `open fun onAddonItemClicked(addon: `[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handler for when an add-on item is clicked. |
| [onInstallAddonButtonClicked](on-install-addon-button-clicked.md) | `open fun onInstallAddonButtonClicked(addon: `[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handler for when the install add-on button is clicked. |
| [onNotYetSupportedSectionClicked](on-not-yet-supported-section-clicked.md) | `open fun onNotYetSupportedSectionClicked(unsupportedAddons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Handler for when the not yet supported section is clicked. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
