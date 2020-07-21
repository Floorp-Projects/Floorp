[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [DefaultLoginValidationDelegate](./index.md)

# DefaultLoginValidationDelegate

`class DefaultLoginValidationDelegate : `[`LoginValidationDelegate`](../../mozilla.components.concept.storage/-login-validation-delegate/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/DefaultLoginValidationDelegate.kt#L22)

A delegate that will check against [storage](#) to see if a given Login can be persisted, and return
information about why it can or cannot.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultLoginValidationDelegate(storage: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`LoginsStorage`](../../mozilla.components.concept.storage/-logins-storage/index.md)`>, scope: CoroutineScope = CoroutineScope(IO))`<br>A delegate that will check against [storage](#) to see if a given Login can be persisted, and return information about why it can or cannot. |

### Functions

| Name | Summary |
|---|---|
| [ensureValidAsync](ensure-valid-async.md) | `fun ensureValidAsync(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): Deferred<`[`Result`](../../mozilla.components.concept.storage/-login-validation-delegate/-result/index.md)`>`<br>Queries underlying storage for a new [Login](../../mozilla.components.concept.storage/-login/index.md) to determine if should be updated or created, and can be merged and saved, or any error states. |
| [getPotentialDupesIgnoringUsernameAsync](get-potential-dupes-ignoring-username-async.md) | `fun getPotentialDupesIgnoringUsernameAsync(newLogin: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>>`<br>Calls underlying storage methods to fetch list of [Login](../../mozilla.components.concept.storage/-login/index.md)s that could be potential dupes of [newLogin](../../mozilla.components.concept.storage/-login-validation-delegate/get-potential-dupes-ignoring-username-async.md#mozilla.components.concept.storage.LoginValidationDelegate$getPotentialDupesIgnoringUsernameAsync(mozilla.components.concept.storage.Login)/newLogin) |
| [shouldUpdateOrCreateAsync](should-update-or-create-async.md) | `fun shouldUpdateOrCreateAsync(newLogin: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`, potentialDupes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>?): Deferred<`[`Result`](../../mozilla.components.concept.storage/-login-validation-delegate/-result/index.md)`>`<br>Compares a [Login](../../mozilla.components.concept.storage/-login/index.md) to a passed in list of potential dupes [Login](../../mozilla.components.concept.storage/-login/index.md) or queries underlying storage for potential dupes list of [Login](../../mozilla.components.concept.storage/-login/index.md) to determine if it should be updated or created. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [mergeWithLogin](merge-with-login.md) | `fun `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`.mergeWithLogin(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`Login`](../../mozilla.components.concept.storage/-login/index.md)<br>Will use values from [login](merge-with-login.md#mozilla.components.service.sync.logins.DefaultLoginValidationDelegate.Companion$mergeWithLogin(mozilla.components.concept.storage.Login, mozilla.components.concept.storage.Login)/login) if they are 1) non-null and 2) non-empty.  Otherwise, will fall back to values from [this](merge-with-login/-this-.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
