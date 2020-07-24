[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [DefaultTabViewHolder](./index.md)

# DefaultTabViewHolder

`class DefaultTabViewHolder : `[`TabViewHolder`](../-tab-view-holder/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/TabViewHolder.kt#L47)

The default implementation of `TabViewHolder`

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultTabViewHolder(itemView: <ERROR CLASS>, thumbnailLoader: `[`ImageLoader`](../../mozilla.components.support.images.loader/-image-loader/index.md)`? = null)`<br>The default implementation of `TabViewHolder` |

### Properties

| Name | Summary |
|---|---|
| [tab](tab.md) | `var tab: `[`Tab`](../../mozilla.components.concept.tabstray/-tab/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [bind](bind.md) | `fun bind(tab: `[`Tab`](../../mozilla.components.concept.tabstray/-tab/index.md)`, isSelected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, styling: `[`TabsTrayStyling`](../-tabs-tray-styling/index.md)`, observable: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Displays the data of the given session and notifies the given observable about events. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
