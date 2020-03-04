[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [DefaultLoginValidationDelegate](index.md) / [validateCanPersist](./validate-can-persist.md)

# validateCanPersist

`fun validateCanPersist(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): Deferred<`[`Result`](../../mozilla.components.concept.storage/-login-validation-delegate/-result/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/DefaultLoginValidationDelegate.kt#L28)

Overrides [LoginValidationDelegate.validateCanPersist](../../mozilla.components.concept.storage/-login-validation-delegate/validate-can-persist.md)

Checks whether or not [login](../../mozilla.components.concept.storage/-login-validation-delegate/validate-can-persist.md#mozilla.components.concept.storage.LoginValidationDelegate$validateCanPersist(mozilla.components.concept.storage.Login)/login) can be persisted.

Note that this method is not thread safe.

**Returns**
a [LoginValidationDelegate.Result](../../mozilla.components.concept.storage/-login-validation-delegate/-result/index.md), detailing whether [login](../../mozilla.components.concept.storage/-login-validation-delegate/validate-can-persist.md#mozilla.components.concept.storage.LoginValidationDelegate$validateCanPersist(mozilla.components.concept.storage.Login)/login) can be saved as a new
value, used to update an existing one, or an error occured.

