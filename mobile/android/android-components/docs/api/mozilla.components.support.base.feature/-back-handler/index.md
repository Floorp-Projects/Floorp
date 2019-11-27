[android-components](../../index.md) / [mozilla.components.support.base.feature](../index.md) / [BackHandler](./index.md)

# BackHandler

`interface ~~BackHandler~~` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/feature/BackHandler.kt#L11)
**Deprecated:** Use `UserInteractionHandler` instead.

Generic interface for fragments, features and other components that want to handle 'back' button presses.

### Functions

| Name | Summary |
|---|---|
| [onBackPressed](on-back-pressed.md) | `abstract fun ~~onBackPressed~~(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Called when this [UserInteractionHandler](../-user-interaction-handler/index.md) gets the option to handle the user pressing the back key. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
