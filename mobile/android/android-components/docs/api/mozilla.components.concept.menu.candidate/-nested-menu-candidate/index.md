[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [NestedMenuCandidate](./index.md)

# NestedMenuCandidate

`data class NestedMenuCandidate : `[`MenuCandidate`](../-menu-candidate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuCandidate.kt#L94)

Menu option that opens a nested sub menu.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `NestedMenuCandidate(id: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, start: `[`MenuIcon`](../-menu-icon.md)`? = null, end: `[`DrawableMenuIcon`](../-drawable-menu-icon/index.md)`? = null, subMenuItems: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`MenuCandidate`](../-menu-candidate/index.md)`>? = emptyList(), textStyle: `[`TextStyle`](../-text-style/index.md)` = TextStyle(), containerStyle: `[`ContainerStyle`](../-container-style/index.md)` = ContainerStyle(), effect: `[`MenuCandidateEffect`](../-menu-candidate-effect.md)`? = null)`<br>Menu option that opens a nested sub menu. |

### Properties

| Name | Summary |
|---|---|
| [containerStyle](container-style.md) | `val containerStyle: `[`ContainerStyle`](../-container-style/index.md)<br>Styling to apply to the container. |
| [effect](effect.md) | `val effect: `[`MenuCandidateEffect`](../-menu-candidate-effect.md)`?`<br>Effects to apply to the option. |
| [end](end.md) | `val end: `[`DrawableMenuIcon`](../-drawable-menu-icon/index.md)`?`<br>Icon to display after the text. |
| [id](id.md) | `val id: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Unique ID for this nested menu. Can be a resource ID. |
| [start](start.md) | `val start: `[`MenuIcon`](../-menu-icon.md)`?`<br>Icon to display before the text. |
| [subMenuItems](sub-menu-items.md) | `val subMenuItems: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`MenuCandidate`](../-menu-candidate/index.md)`>?`<br>Nested menu items to display. If null, this item will instead return to the root menu. |
| [text](text.md) | `val text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Text to display. |
| [textStyle](text-style.md) | `val textStyle: `[`TextStyle`](../-text-style/index.md)<br>Styling to apply to the text. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
