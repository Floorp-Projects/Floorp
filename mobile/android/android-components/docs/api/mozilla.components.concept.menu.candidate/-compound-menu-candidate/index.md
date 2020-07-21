[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [CompoundMenuCandidate](./index.md)

# CompoundMenuCandidate

`data class CompoundMenuCandidate : `[`MenuCandidate`](../-menu-candidate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuCandidate.kt#L61)

Menu option that shows a switch or checkbox.

### Types

| Name | Summary |
|---|---|
| [ButtonType](-button-type/index.md) | `enum class ButtonType`<br>Compound button types to display with the compound menu option. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CompoundMenuCandidate(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isChecked: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, start: `[`MenuIcon`](../-menu-icon.md)`? = null, end: `[`ButtonType`](-button-type/index.md)`, textStyle: `[`TextStyle`](../-text-style/index.md)` = TextStyle(), containerStyle: `[`ContainerStyle`](../-container-style/index.md)` = ContainerStyle(), effect: `[`MenuCandidateEffect`](../-menu-candidate-effect.md)`? = null, onCheckedChange: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`<br>Menu option that shows a switch or checkbox. |

### Properties

| Name | Summary |
|---|---|
| [containerStyle](container-style.md) | `val containerStyle: `[`ContainerStyle`](../-container-style/index.md)<br>Styling to apply to the container. |
| [effect](effect.md) | `val effect: `[`MenuCandidateEffect`](../-menu-candidate-effect.md)`?`<br>Effects to apply to the option. |
| [end](end.md) | `val end: `[`ButtonType`](-button-type/index.md)<br>Compound button to display after the text. |
| [isChecked](is-checked.md) | `val isChecked: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [onCheckedChange](on-checked-change.md) | `val onCheckedChange: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Listener called when this menu option is checked or unchecked. |
| [start](start.md) | `val start: `[`MenuIcon`](../-menu-icon.md)`?`<br>Icon to display before the text. |
| [text](text.md) | `val text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Text to display. |
| [textStyle](text-style.md) | `val textStyle: `[`TextStyle`](../-text-style/index.md)<br>Styling to apply to the text. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
