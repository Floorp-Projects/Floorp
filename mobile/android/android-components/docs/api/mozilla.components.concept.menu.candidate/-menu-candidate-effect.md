[android-components](../index.md) / [mozilla.components.concept.menu.candidate](index.md) / [MenuCandidateEffect](./-menu-candidate-effect.md)

# MenuCandidateEffect

`sealed class MenuCandidateEffect : `[`MenuEffect`](-menu-effect.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuEffect.kt#L19)

Describes an effect for a menu candidate and its container.
Effects can also alter the button that opens the menu.

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [HighPriorityHighlightEffect](-high-priority-highlight-effect/index.md) | `data class HighPriorityHighlightEffect : `[`MenuCandidateEffect`](./-menu-candidate-effect.md)<br>Changes the background of the menu item. Used for errors that require user attention, like sync errors. |
