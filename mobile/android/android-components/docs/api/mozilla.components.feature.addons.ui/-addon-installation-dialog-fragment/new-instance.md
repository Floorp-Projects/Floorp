[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [AddonInstallationDialogFragment](index.md) / [newInstance](./new-instance.md)

# newInstance

`fun newInstance(addon: `[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`, addonCollectionProvider: `[`AddonCollectionProvider`](../../mozilla.components.feature.addons.amo/-addon-collection-provider/index.md)`, promptsStyling: `[`PromptsStyling`](-prompts-styling/index.md)`? = PromptsStyling(
                gravity = Gravity.BOTTOM,
                shouldWidthMatchParent = true
            ), onConfirmButtonClicked: (`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`, `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`AddonInstallationDialogFragment`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/AddonInstallationDialogFragment.kt#L250)

Returns a new instance of [AddonInstallationDialogFragment](index.md).

### Parameters

`addon` - The addon to show in the dialog.

`promptsStyling` - Styling properties for the dialog.

`onConfirmButtonClicked` - A lambda called when the confirm button is clicked.