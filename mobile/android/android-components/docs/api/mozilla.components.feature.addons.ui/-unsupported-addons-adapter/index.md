[android-components](../../index.md) / [mozilla.components.feature.addons.ui](../index.md) / [UnsupportedAddonsAdapter](./index.md)

# UnsupportedAddonsAdapter

`class UnsupportedAddonsAdapter : Adapter<ViewHolder>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/ui/UnsupportedAddonsAdapter.kt#L27)

An adapter for displaying unsupported add-on items.

### Types

| Name | Summary |
|---|---|
| [UnsupportedAddonViewHolder](-unsupported-addon-view-holder/index.md) | `class UnsupportedAddonViewHolder : ViewHolder`<br>A view holder for displaying unsupported add-on items. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UnsupportedAddonsAdapter(addonManager: `[`AddonManager`](../../mozilla.components.feature.addons/-addon-manager/index.md)`, unsupportedAddonsAdapterDelegate: `[`UnsupportedAddonsAdapterDelegate`](../-unsupported-addons-adapter-delegate/index.md)`, addons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Addon`](../../mozilla.components.feature.addons/-addon/index.md)`>)`<br>An adapter for displaying unsupported add-on items. |

### Functions

| Name | Summary |
|---|---|
| [getItemCount](get-item-count.md) | `fun getItemCount(): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [onBindViewHolder](on-bind-view-holder.md) | `fun onBindViewHolder(holder: ViewHolder, position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onCreateViewHolder](on-create-view-holder.md) | `fun onCreateViewHolder(parent: <ERROR CLASS>, viewType: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): ViewHolder` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
