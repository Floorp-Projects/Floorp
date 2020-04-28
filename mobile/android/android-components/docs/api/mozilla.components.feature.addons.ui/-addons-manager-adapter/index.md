[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [AddonsManagerAdapter](./index.md)

# AddonsManagerAdapter

`class AddonsManagerAdapter : ListAdapter<`[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`, `[`CustomViewHolder`](../-custom-view-holder/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/AddonsManagerAdapter.kt#L56)

An adapter for displaying add-on items. This will display information related to the state of
an add-on such as recommended, unsupported or installed. In addition, it will perform actions
such as installing an add-on.

### Types

| Name | Summary |
|---|---|
| [Style](-style/index.md) | `data class Style`<br>Allows to customize how items should look like. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddonsManagerAdapter(addonCollectionProvider: `[`AddonCollectionProvider`](../../mozilla.components.feature.addons.amo/-addon-collection-provider/index.md)`, addonsManagerDelegate: `[`AddonsManagerAdapterDelegate`](../-addons-manager-adapter-delegate/index.md)`, addons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`>, style: `[`Style`](-style/index.md)`? = null)`<br>An adapter for displaying add-on items. This will display information related to the state of an add-on such as recommended, unsupported or installed. In addition, it will perform actions such as installing an add-on. |

### Functions

| Name | Summary |
|---|---|
| [getItemViewType](get-item-view-type.md) | `fun getItemViewType(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [onBindViewHolder](on-bind-view-holder.md) | `fun onBindViewHolder(holder: `[`CustomViewHolder`](../-custom-view-holder/index.md)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreateViewHolder](on-create-view-holder.md) | `fun onCreateViewHolder(parent: <ERROR CLASS>, viewType: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`CustomViewHolder`](../-custom-view-holder/index.md) |
| [updateAddon](update-addon.md) | `fun updateAddon(addon: `[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Update the portion of the list that contains the provided [addon](update-addon.md#mozilla.components.feature.addons.ui.AddonsManagerAdapter$updateAddon(mozilla.components.feature.addons.Addon)/addon). |
| [updateAddons](update-addons.md) | `fun updateAddons(addons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates only the portion of the list that changes between the current list and the new provided [addons](update-addons.md#mozilla.components.feature.addons.ui.AddonsManagerAdapter$updateAddons(kotlin.collections.List((mozilla.components.feature.addons.Addon)))/addons). Be aware that updating a subset of the visible list is not supported, [addons](update-addons.md#mozilla.components.feature.addons.ui.AddonsManagerAdapter$updateAddons(kotlin.collections.List((mozilla.components.feature.addons.Addon)))/addons) will replace the current list, but only the add-ons that have been changed will be updated in the UI. If you provide a subset it will replace the current list. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
