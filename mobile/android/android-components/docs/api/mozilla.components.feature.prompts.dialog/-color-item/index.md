[android-components](../../index.md) / [mozilla.components.feature.prompts.dialog](../index.md) / [ColorItem](./index.md)

# ColorItem

`data class ColorItem` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/dialog/BasicColorAdapter.kt#L33)

Represents an item in the [BasicColorAdapter](#) list.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ColorItem(color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Represents an item in the [BasicColorAdapter](#) list. |

### Properties

| Name | Summary |
|---|---|
| [color](color.md) | `val color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>color int that this item corresponds to. |
| [contentDescription](content-description.md) | `val contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>accessibility description of this color. |
| [selected](selected.md) | `val selected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>if true, this is the color that will be set when the dialog is closed. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
