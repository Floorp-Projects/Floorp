[android-components](../../index.md) / [mozilla.components.feature.logins.exceptions](../index.md) / [LoginExceptionStorage](./index.md)

# LoginExceptionStorage

`class LoginExceptionStorage : `[`LoginExceptions`](../../mozilla.components.feature.prompts.login/-login-exceptions/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/logins/src/main/java/mozilla/components/feature/logins/exceptions/LoginExceptionStorage.kt#L19)

A storage implementation for organizing login exceptions.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LoginExceptionStorage(context: <ERROR CLASS>)`<br>A storage implementation for organizing login exceptions. |

### Functions

| Name | Summary |
|---|---|
| [addLoginException](add-login-exception.md) | `fun addLoginException(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new [LoginException](../-login-exception/index.md). |
| [deleteAllLoginExceptions](delete-all-login-exceptions.md) | `fun deleteAllLoginExceptions(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes all [LoginException](../-login-exception/index.md)s. |
| [findExceptionByOrigin](find-exception-by-origin.md) | `fun findExceptionByOrigin(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`LoginException`](../-login-exception/index.md)`?`<br>Finds a [LoginException](../-login-exception/index.md) by origin. |
| [getLoginExceptions](get-login-exceptions.md) | `fun getLoginExceptions(): Flow<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`LoginException`](../-login-exception/index.md)`>>`<br>Returns a [Flow](#) list of all the [LoginException](../-login-exception/index.md) instances. |
| [getLoginExceptionsPaged](get-login-exceptions-paged.md) | `fun getLoginExceptionsPaged(): Factory<`[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, `[`LoginException`](../-login-exception/index.md)`>`<br>Returns all [LoginException](../-login-exception/index.md)s as a [DataSource.Factory](#). |
| [isLoginExceptionByOrigin](is-login-exception-by-origin.md) | `fun isLoginExceptionByOrigin(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if a specific origin should show a save logins prompt or if it is an exception. |
| [removeLoginException](remove-login-exception.md) | `fun removeLoginException(site: `[`LoginException`](../-login-exception/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the given [LoginException](../-login-exception/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
