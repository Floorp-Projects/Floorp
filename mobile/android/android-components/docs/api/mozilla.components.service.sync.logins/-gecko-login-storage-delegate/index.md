[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [GeckoLoginStorageDelegate](./index.md)

# GeckoLoginStorageDelegate

`class GeckoLoginStorageDelegate : `[`LoginStorageDelegate`](../../mozilla.components.concept.storage/-login-storage-delegate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/GeckoLoginStorageDelegate.kt#L56)

[LoginStorageDelegate](../../mozilla.components.concept.storage/-login-storage-delegate/index.md) implementation.

An abstraction that handles the persistence and retrieval of [LoginEntry](#)s so that Gecko doesn't
have to.

In order to use this class, attach it to the active [GeckoRuntime](#) as its `loginStorageDelegate`.
It is not designed to work with other engines.

This class is part of a complex flow integrating Gecko and Application Services code, which is
described here:

* GV finds something on a page that it believes could be autofilled
* GV calls [onLoginFetch](on-login-fetch.md)
* We retrieve all [Login](../../mozilla.components.concept.storage/-login/index.md)s with matching domains (if any) from [loginStorage](#)
* We return these [Login](../../mozilla.components.concept.storage/-login/index.md)s to GV
* GV autofills one of the returned [Login](../../mozilla.components.concept.storage/-login/index.md)s into the page
* GV calls [onLoginUsed](on-login-used.md) to let us know which [Login](../../mozilla.components.concept.storage/-login/index.md) was used
* User submits their credentials
* GV detects something that looks like a login submission
* ([GeckoLoginStorageDelegate](./index.md) is not involved with this step) A prompt is shown to the user,
who decides whether or not to save the [Login](../../mozilla.components.concept.storage/-login/index.md)
  * If the user declines, break
* GV calls [onLoginSave](on-login-save.md) with a [Login](../../mozilla.components.concept.storage/-login/index.md)
  * If this [Login](../../mozilla.components.concept.storage/-login/index.md) was autofilled, it is updated with new information but retains the same
    [Login.guid](../../mozilla.components.concept.storage/-login/guid.md)
  * If this is a new [Login](../../mozilla.components.concept.storage/-login/index.md), its [Login.guid](../../mozilla.components.concept.storage/-login/guid.md) will be an empty string
* We [CREATE](#) or [UPDATE](#) the [Login](../../mozilla.components.concept.storage/-login/index.md) in [loginStorage](#)
  * If [CREATE](#), [loginStorage](#) generates a new guid for it

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoLoginStorageDelegate(loginStorage: `[`AsyncLoginsStorage`](../-async-logins-storage/index.md)`, keyStore: `[`SecureAbove22Preferences`](../../mozilla.components.lib.dataprotect/-secure-above22-preferences/index.md)`, isAutofillEnabled: () -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { false }, scope: CoroutineScope = CoroutineScope(Dispatchers.IO))`<br>[LoginStorageDelegate](../../mozilla.components.concept.storage/-login-storage-delegate/index.md) implementation. |

### Functions

| Name | Summary |
|---|---|
| [getPersistenceOperation](get-persistence-operation.md) | `fun getPersistenceOperation(newLogin: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`, savedLogin: `[`ServerPassword`](../-server-password.md)`?): `[`Operation`](../-operation/index.md)<br>Returns whether an existing login record should be [UPDATE](#)d or a new one [CREATE](#)d, based on the saved [ServerPassword](../-server-password.md) and new [Login](../../mozilla.components.concept.storage/-login/index.md). |
| [onLoginFetch](on-login-fetch.md) | `fun onLoginFetch(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>`<br>Given a [domain](../../mozilla.components.concept.storage/-login-storage-delegate/on-login-fetch.md#mozilla.components.concept.storage.LoginStorageDelegate$onLoginFetch(kotlin.String)/domain), returns a [GeckoResult](#) of the matching [LoginEntry](#)s found in [loginStorage](#). |
| [onLoginSave](on-login-save.md) | `fun onLoginSave(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called when a [login](../../mozilla.components.concept.storage/-login-storage-delegate/on-login-save.md#mozilla.components.concept.storage.LoginStorageDelegate$onLoginSave(mozilla.components.concept.storage.Login)/login) should be saved or updated. |
| [onLoginUsed](on-login-used.md) | `fun onLoginUsed(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after a [login](../../mozilla.components.concept.storage/-login-storage-delegate/on-login-used.md#mozilla.components.concept.storage.LoginStorageDelegate$onLoginUsed(mozilla.components.concept.storage.Login)/login) has been autofilled into web content. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
