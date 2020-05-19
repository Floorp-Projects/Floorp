[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [TextMenuCandidate](./index.md)

# TextMenuCandidate

`data class TextMenuCandidate : `[`MenuCandidate`](../-menu-candidate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuCandidate.kt#L25)

Interactive menu option that displays some text.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TextMenuCandidate(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, start: `[`MenuIcon`](../-menu-icon.md)`? = null, end: `[`MenuIcon`](../-menu-icon.md)`? = null, textStyle: `[`TextStyle`](../-text-style/index.md)` = TextStyle(), containerStyle: `[`ContainerStyle`](../-container-style/index.md)` = ContainerStyle(), effect: `[`MenuCandidateEffect`](../-menu-candidate-effect.md)`? = null, onClick: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`<br>Interactive menu option that displays some text. |

### Properties

| Name | Summary |
|---|---|
| [containerStyle](container-style.md) | `val containerStyle: `[`ContainerStyle`](../-container-style/index.md)<br>Styling to apply to the container. |
| [effect](effect.md) | `val effect: `[`MenuCandidateEffect`](../-menu-candidate-effect.md)`?`<br>Effects to apply to the option. |
| [end](end.md) | `val end: `[`MenuIcon`](../-menu-icon.md)`?`<br>Icon to display after the text. |
| [onClick](on-click.md) | `val onClick: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Click listener called when this menu option is clicked. |
| [start](start.md) | `val start: `[`MenuIcon`](../-menu-icon.md)`?`<br>Icon to display before the text. |
| [text](text.md) | `val text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Text to display. |
| [textStyle](text-style.md) | `val textStyle: `[`TextStyle`](../-text-style/index.md)<br>Styling to apply to the text. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
