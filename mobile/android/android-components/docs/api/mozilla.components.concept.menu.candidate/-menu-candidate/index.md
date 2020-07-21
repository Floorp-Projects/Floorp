[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [MenuCandidate](./index.md)

# MenuCandidate

`sealed class MenuCandidate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuCandidate.kt#L10)

Menu option data classes to be shown in the browser menu.

### Properties

| Name | Summary |
|---|---|
| [containerStyle](container-style.md) | `abstract val containerStyle: `[`ContainerStyle`](../-container-style/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [CompoundMenuCandidate](../-compound-menu-candidate/index.md) | `data class CompoundMenuCandidate : `[`MenuCandidate`](./index.md)<br>Menu option that shows a switch or checkbox. |
| [DecorativeTextMenuCandidate](../-decorative-text-menu-candidate/index.md) | `data class DecorativeTextMenuCandidate : `[`MenuCandidate`](./index.md)<br>Menu option that displays static text. |
| [DividerMenuCandidate](../-divider-menu-candidate/index.md) | `data class DividerMenuCandidate : `[`MenuCandidate`](./index.md)<br>Menu option to display a horizontal divider. |
| [NestedMenuCandidate](../-nested-menu-candidate/index.md) | `data class NestedMenuCandidate : `[`MenuCandidate`](./index.md)<br>Menu option that opens a nested sub menu. |
| [RowMenuCandidate](../-row-menu-candidate/index.md) | `data class RowMenuCandidate : `[`MenuCandidate`](./index.md)<br>Displays a row of small menu options. |
| [TextMenuCandidate](../-text-menu-candidate/index.md) | `data class TextMenuCandidate : `[`MenuCandidate`](./index.md)<br>Interactive menu option that displays some text. |
