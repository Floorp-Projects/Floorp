[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [DefaultLoginValidationDelegate](./index.md)

# DefaultLoginValidationDelegate

`class DefaultLoginValidationDelegate : `[`LoginValidationDelegate`](../../mozilla.components.concept.storage/-login-validation-delegate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/DefaultLoginValidationDelegate.kt#L22)

A delegate that will check against [storage](#) to see if a given Login can be persisted, and return
information about why it can or cannot.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultLoginValidationDelegate(storage: `[`LoginsStorage`](../../mozilla.components.concept.storage/-logins-storage/index.md)`, scope: CoroutineScope = CoroutineScope(IO))`<br>A delegate that will check against [storage](#) to see if a given Login can be persisted, and return information about why it can or cannot. |

### Functions

| Name | Summary |
|---|---|
| [validateCanPersist](validate-can-persist.md) | `fun validateCanPersist(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): Deferred<`[`Result`](../../mozilla.components.concept.storage/-login-validation-delegate/-result/index.md)`>`<br>Checks whether or not [login](../../mozilla.components.concept.storage/-login-validation-delegate/validate-can-persist.md#mozilla.components.concept.storage.LoginValidationDelegate$validateCanPersist(mozilla.components.concept.storage.Login)/login) can be persisted. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
