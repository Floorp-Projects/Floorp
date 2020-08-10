[android-components](../../index.md) / [mozilla.components.feature.logins.exceptions](../index.md) / [LoginException](./index.md)

# LoginException

`interface LoginException` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/logins/src/main/java/mozilla/components/feature/logins/exceptions/LoginException.kt#L10)

A login exception.

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `abstract val id: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Unique ID of this login exception. |
| [origin](origin.md) | `abstract val origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The origin of the login exception site. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
