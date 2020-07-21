[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginValidationDelegate](./index.md)

# LoginValidationDelegate

`interface LoginValidationDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L190)

Provides a method for checking whether or not a given login can be stored.

### Types

| Name | Summary |
|---|---|
| [Result](-result/index.md) | `sealed class Result`<br>The result of validating a given [Login](../-login/index.md) against currently stored [Login](../-login/index.md)s.  This will include whether it can be created, updated, or neither, along with an explanation of any errors. |

### Functions

| Name | Summary |
|---|---|
| [getPotentialDupesIgnoringUsernameAsync](get-potential-dupes-ignoring-username-async.md) | `abstract fun getPotentialDupesIgnoringUsernameAsync(newLogin: `[`Login`](../-login/index.md)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>>`<br>Calls underlying storage methods to fetch list of [Login](../-login/index.md)s that could be potential dupes of [newLogin](get-potential-dupes-ignoring-username-async.md#mozilla.components.concept.storage.LoginValidationDelegate$getPotentialDupesIgnoringUsernameAsync(mozilla.components.concept.storage.Login)/newLogin) |
| [shouldUpdateOrCreateAsync](should-update-or-create-async.md) | `abstract fun shouldUpdateOrCreateAsync(newLogin: `[`Login`](../-login/index.md)`, potentialDupes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>? = null): Deferred<`[`Result`](-result/index.md)`>`<br>Checks whether a [login](#) should be saved or updated. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultLoginValidationDelegate](../../mozilla.components.service.sync.logins/-default-login-validation-delegate/index.md) | `class DefaultLoginValidationDelegate : `[`LoginValidationDelegate`](./index.md)<br>A delegate that will check against [storage](#) to see if a given Login can be persisted, and return information about why it can or cannot. |
