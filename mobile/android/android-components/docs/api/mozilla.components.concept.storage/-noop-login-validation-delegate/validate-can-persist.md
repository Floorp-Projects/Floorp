[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [NoopLoginValidationDelegate](index.md) / [validateCanPersist](./validate-can-persist.md)

# validateCanPersist

`fun validateCanPersist(login: `[`Login`](../-login/index.md)`): Deferred<`[`Result`](../-login-validation-delegate/-result/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L105)

Overrides [LoginValidationDelegate.validateCanPersist](../-login-validation-delegate/validate-can-persist.md)

Checks whether or not [login](../-login-validation-delegate/validate-can-persist.md#mozilla.components.concept.storage.LoginValidationDelegate$validateCanPersist(mozilla.components.concept.storage.Login)/login) can be persisted.

**Returns**
a [LoginValidationDelegate.Result](../-login-validation-delegate/-result/index.md), detailing whether [login](../-login-validation-delegate/validate-can-persist.md#mozilla.components.concept.storage.LoginValidationDelegate$validateCanPersist(mozilla.components.concept.storage.Login)/login) can be saved as a new
value, used to update an existing one, or an error occured.

