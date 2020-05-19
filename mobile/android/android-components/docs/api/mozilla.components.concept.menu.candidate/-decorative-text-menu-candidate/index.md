[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [DecorativeTextMenuCandidate](./index.md)

# DecorativeTextMenuCandidate

`data class DecorativeTextMenuCandidate : `[`MenuCandidate`](../-menu-candidate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuCandidate.kt#L43)

Menu option that displays static text.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DecorativeTextMenuCandidate(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, textStyle: `[`TextStyle`](../-text-style/index.md)` = TextStyle(), containerStyle: `[`ContainerStyle`](../-container-style/index.md)` = ContainerStyle())`<br>Menu option that displays static text. |

### Properties

| Name | Summary |
|---|---|
| [containerStyle](container-style.md) | `val containerStyle: `[`ContainerStyle`](../-container-style/index.md)<br>Styling to apply to the container. |
| [height](height.md) | `val height: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>Custom height for the menu option. |
| [text](text.md) | `val text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Text to display. |
| [textStyle](text-style.md) | `val textStyle: `[`TextStyle`](../-text-style/index.md)<br>Styling to apply to the text. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
