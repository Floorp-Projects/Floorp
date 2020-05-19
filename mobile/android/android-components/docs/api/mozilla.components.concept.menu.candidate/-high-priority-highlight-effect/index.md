[android-components](../../index.md) / [mozilla.components.concept.menu.candidate](../index.md) / [HighPriorityHighlightEffect](./index.md)

# HighPriorityHighlightEffect

`data class HighPriorityHighlightEffect : `[`MenuCandidateEffect`](../-menu-candidate-effect.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuEffect.kt#L44)

Changes the background of the menu item.
Used for errors that require user attention, like sync errors.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `HighPriorityHighlightEffect(backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Changes the background of the menu item. Used for errors that require user attention, like sync errors. |

### Properties

| Name | Summary |
|---|---|
| [backgroundTint](background-tint.md) | `val backgroundTint: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Tint for the menu item background color. Also used to highlight the menu button. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
