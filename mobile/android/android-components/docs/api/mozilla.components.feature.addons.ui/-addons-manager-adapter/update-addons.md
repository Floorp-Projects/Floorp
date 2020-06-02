[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [AddonsManagerAdapter](index.md) / [updateAddons](./update-addons.md)

# updateAddons

`fun updateAddons(addons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/AddonsManagerAdapter.kt#L391)

Updates only the portion of the list that changes between the current list and the new provided [addons](update-addons.md#mozilla.components.feature.addons.ui.AddonsManagerAdapter$updateAddons(kotlin.collections.List((mozilla.components.feature.addons.Addon)))/addons).
Be aware that updating a subset of the visible list is not supported, [addons](update-addons.md#mozilla.components.feature.addons.ui.AddonsManagerAdapter$updateAddons(kotlin.collections.List((mozilla.components.feature.addons.Addon)))/addons) will replace
the current list, but only the add-ons that have been changed will be updated in the UI.
If you provide a subset it will replace the current list.

