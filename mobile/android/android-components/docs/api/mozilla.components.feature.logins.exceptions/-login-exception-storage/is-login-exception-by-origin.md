[android-components](../../index.md) / [mozilla.components.feature.logins.exceptions](../index.md) / [LoginExceptionStorage](index.md) / [isLoginExceptionByOrigin](./is-login-exception-by-origin.md)

# isLoginExceptionByOrigin

`fun isLoginExceptionByOrigin(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/logins/src/main/java/mozilla/components/feature/logins/exceptions/LoginExceptionStorage.kt#L63)

Overrides [LoginExceptions.isLoginExceptionByOrigin](../../mozilla.components.feature.prompts/-login-exceptions/is-login-exception-by-origin.md)

Checks if a specific origin should show a save logins prompt or if it is an exception.

### Parameters

`origin` - The origin to search exceptions list for.