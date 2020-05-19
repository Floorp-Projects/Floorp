[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [TextMenuIcon](./index.md)

# TextMenuIcon

`data class TextMenuIcon : `[`MenuIcon`](../-menu-icon.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuIcon.kt#L67)

Menu icon to display additional text at the end of a menu option.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TextMenuIcon(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, textStyle: `[`TextStyle`](../-text-style/index.md)` = TextStyle())`<br>Menu icon to display additional text at the end of a menu option. |

### Properties

| Name | Summary |
|---|---|
| [backgroundTint](background-tint.md) | `val backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>Color to show behind text. |
| [text](text.md) | `val text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Text to display. |
| [textStyle](text-style.md) | `val textStyle: `[`TextStyle`](../-text-style/index.md)<br>Styling to apply to the text. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
