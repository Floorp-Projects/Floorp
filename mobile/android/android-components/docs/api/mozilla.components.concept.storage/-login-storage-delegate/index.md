[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginStorageDelegate](./index.md)

# LoginStorageDelegate

`interface LoginStorageDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L256)

Used to handle [Login](../-login/index.md) storage so that the underlying engine doesn't have to. An instance of
this should be attached to the Gecko runtime in order to be used.

### Functions

| Name | Summary |
|---|---|
| [onLoginFetch](on-login-fetch.md) | `abstract fun onLoginFetch(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>>`<br>Given a [domain](on-login-fetch.md#mozilla.components.concept.storage.LoginStorageDelegate$onLoginFetch(kotlin.String)/domain), returns a [GeckoResult](#) of the matching [LoginEntry](#)s found in [loginStorage](#). |
| [onLoginSave](on-login-save.md) | `abstract fun onLoginSave(login: `[`Login`](../-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when a [login](on-login-save.md#mozilla.components.concept.storage.LoginStorageDelegate$onLoginSave(mozilla.components.concept.storage.Login)/login) should be saved or updated. |
| [onLoginUsed](on-login-used.md) | `abstract fun onLoginUsed(login: `[`Login`](../-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after a [login](on-login-used.md#mozilla.components.concept.storage.LoginStorageDelegate$onLoginUsed(mozilla.components.concept.storage.Login)/login) has been autofilled into web content. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoLoginStorageDelegate](../../mozilla.components.service.sync.logins/-gecko-login-storage-delegate/index.md) | `class GeckoLoginStorageDelegate : `[`LoginStorageDelegate`](./index.md)<br>[LoginStorageDelegate](./index.md) implementation. |
