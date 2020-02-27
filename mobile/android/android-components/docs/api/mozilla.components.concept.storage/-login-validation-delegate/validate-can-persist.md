[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginValidationDelegate](index.md) / [validateCanPersist](./validate-can-persist.md)

# validateCanPersist

`abstract fun validateCanPersist(login: `[`Login`](../-login/index.md)`): Deferred<`[`Result`](-result/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L94)

Checks whether or not [login](validate-can-persist.md#mozilla.components.concept.storage.LoginValidationDelegate$validateCanPersist(mozilla.components.concept.storage.Login)/login) can be persisted.

Note that this method is not thread safe.

**Returns**
a [LoginValidationDelegate.Result](-result/index.md), detailing whether [login](validate-can-persist.md#mozilla.components.concept.storage.LoginValidationDelegate$validateCanPersist(mozilla.components.concept.storage.Login)/login) can be saved as a new
value, used to update an existing one, or an error occured.

