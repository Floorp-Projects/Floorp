[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [LoginExceptions](./index.md)

# LoginExceptions

`interface LoginExceptions` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/LoginExceptions.kt#L10)

Interface to be implemented by a storage layer to exclude the save logins prompt from showing.

### Functions

| Name | Summary |
|---|---|
| [addLoginException](add-login-exception.md) | `abstract fun addLoginException(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new origin to the exceptions list implementation. |
| [isLoginExceptionByOrigin](is-login-exception-by-origin.md) | `abstract fun isLoginExceptionByOrigin(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if a specific origin should show a save logins prompt or if it is an exception. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [LoginExceptionStorage](../../mozilla.components.feature.logins.exceptions/-login-exception-storage/index.md) | `class LoginExceptionStorage : `[`LoginExceptions`](./index.md)<br>A storage implementation for organizing login exceptions. |
