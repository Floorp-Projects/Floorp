[android-components](../../index.md) / [mozilla.components.feature.prompts](../index.md) / [LoginExceptions](index.md) / [isLoginExceptionByOrigin](./is-login-exception-by-origin.md)

# isLoginExceptionByOrigin

`abstract fun isLoginExceptionByOrigin(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/prompts/src/main/java/mozilla/components/feature/prompts/LoginExceptions.kt#L15)

Checks if a specific origin should show a save logins prompt or if it is an exception.

### Parameters

`origin` - The origin to search exceptions list for.