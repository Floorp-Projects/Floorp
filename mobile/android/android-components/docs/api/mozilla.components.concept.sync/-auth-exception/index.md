[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [AuthException](./index.md)

# AuthException

`class AuthException : `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L35)

An exception which may happen while obtaining auth information using [OAuthAccount](../-o-auth-account/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AuthException(type: `[`AuthExceptionType`](../-auth-exception-type/index.md)`, cause: `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`? = null)`<br>An exception which may happen while obtaining auth information using [OAuthAccount](../-o-auth-account/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [getStacktraceAsString](../../mozilla.components.support.base.ext/kotlin.-throwable/get-stacktrace-as-string.md) | `fun `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`.getStacktraceAsString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a formatted string of the [Throwable.stackTrace](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/stack-trace.html). |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
