[android-components](../index.md) / [mozilla.components.concept.menu.candidate](index.md) / [MenuEffect](./-menu-effect.md)

# MenuEffect

`sealed class MenuEffect` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/menu/src/main/java/mozilla/components/concept/menu/candidate/MenuEffect.kt#L13)

Describes an effect for the menu.
Effects can also alter the button to open the menu.

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [MenuCandidateEffect](-menu-candidate-effect.md) | `sealed class MenuCandidateEffect : `[`MenuEffect`](./-menu-effect.md)<br>Describes an effect for a menu candidate and its container. Effects can also alter the button that opens the menu. |
| [MenuIconEffect](-menu-icon-effect.md) | `sealed class MenuIconEffect : `[`MenuEffect`](./-menu-effect.md)<br>Describes an effect for a menu icon. Effects can also alter the button that opens the menu. |
