[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [RowMenuCandidate](./index.md)

# RowMenuCandidate

`data class RowMenuCandidate : `[`MenuCandidate`](../-menu-candidate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuCandidate.kt#L111)

Displays a row of small menu options.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RowMenuCandidate(items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SmallMenuCandidate`](../-small-menu-candidate/index.md)`>, containerStyle: `[`ContainerStyle`](../-container-style/index.md)` = ContainerStyle())`<br>Displays a row of small menu options. |

### Properties

| Name | Summary |
|---|---|
| [containerStyle](container-style.md) | `val containerStyle: `[`ContainerStyle`](../-container-style/index.md)<br>Styling to apply to the container. |
| [items](items.md) | `val items: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`SmallMenuCandidate`](../-small-menu-candidate/index.md)`>`<br>Small menu options to display. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
