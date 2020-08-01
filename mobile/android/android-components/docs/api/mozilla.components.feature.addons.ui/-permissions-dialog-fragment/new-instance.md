[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [PermissionsDialogFragment](index.md) / [newInstance](./new-instance.md)

# newInstance

`fun newInstance(addon: `[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`, promptsStyling: `[`PromptsStyling`](-prompts-styling/index.md)`? = PromptsStyling(
                gravity = Gravity.BOTTOM,
                shouldWidthMatchParent = true
            ), onPositiveButtonClicked: (`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null, onNegativeButtonClicked: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null): `[`PermissionsDialogFragment`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/PermissionsDialogFragment.kt#L205)

Returns a new instance of [PermissionsDialogFragment](index.md).

### Parameters

`addon` - The addon to show in the dialog.

`promptsStyling` - Styling properties for the dialog.

`onPositiveButtonClicked` - A lambda called when the allow button is clicked.

`onNegativeButtonClicked` - A lambda called when the deny button is clicked.