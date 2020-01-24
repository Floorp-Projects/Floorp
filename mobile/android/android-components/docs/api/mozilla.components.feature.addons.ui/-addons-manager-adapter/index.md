[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [AddonsManagerAdapter](./index.md)

# AddonsManagerAdapter

`class AddonsManagerAdapter : Adapter<`[`CustomViewHolder`](../-custom-view-holder/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/AddonsManagerAdapter.kt#L42)

An adapter for displaying add-on items. This will display information related to the state of
an add-on such as recommended, unsupported or installed. In addition, it will perform actions
such as installing an add-on.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AddonsManagerAdapter(addonCollectionProvider: `[`AddonCollectionProvider`](../../mozilla.components.feature.addons.amo/-addon-collection-provider/index.md)`, addonsManagerDelegate: `[`AddonsManagerAdapterDelegate`](../-addons-manager-adapter-delegate/index.md)`, addons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`>)`<br>An adapter for displaying add-on items. This will display information related to the state of an add-on such as recommended, unsupported or installed. In addition, it will perform actions such as installing an add-on. |

### Functions

| Name | Summary |
|---|---|
| [getItemCount](get-item-count.md) | `fun getItemCount(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [getItemViewType](get-item-view-type.md) | `fun getItemViewType(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [onBindViewHolder](on-bind-view-holder.md) | `fun onBindViewHolder(holder: `[`CustomViewHolder`](../-custom-view-holder/index.md)`, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreateViewHolder](on-create-view-holder.md) | `fun onCreateViewHolder(parent: <ERROR CLASS>, viewType: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`CustomViewHolder`](../-custom-view-holder/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
