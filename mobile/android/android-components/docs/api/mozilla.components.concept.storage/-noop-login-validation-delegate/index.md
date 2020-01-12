[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [NoopLoginValidationDelegate](./index.md)

# NoopLoginValidationDelegate

`class NoopLoginValidationDelegate : `[`LoginValidationDelegate`](../-login-validation-delegate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L104)

Default [LoginValidationDelegate](../-login-validation-delegate/index.md) implementation that always returns false.

This can be used by any consumer that does not want to make use of autofill APIs.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `NoopLoginValidationDelegate()`<br>Default [LoginValidationDelegate](../-login-validation-delegate/index.md) implementation that always returns false. |

### Functions

| Name | Summary |
|---|---|
| [validateCanPersist](validate-can-persist.md) | `fun validateCanPersist(login: `[`Login`](../-login/index.md)`): Deferred<`[`Result`](../-login-validation-delegate/-result/index.md)`>`<br>Checks whether or not [login](../-login-validation-delegate/validate-can-persist.md#mozilla.components.concept.storage.LoginValidationDelegate$validateCanPersist(mozilla.components.concept.storage.Login)/login) can be persisted. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
