[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginValidationDelegate](index.md) / [getPotentialDupesIgnoringUsernameAsync](./get-potential-dupes-ignoring-username-async.md)

# getPotentialDupesIgnoringUsernameAsync

`abstract fun getPotentialDupesIgnoringUsernameAsync(newLogin: `[`Login`](../-login/index.md)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L239)

Calls underlying storage methods to fetch list of [Login](../-login/index.md)s that could be potential dupes of [newLogin](get-potential-dupes-ignoring-username-async.md#mozilla.components.concept.storage.LoginValidationDelegate$getPotentialDupesIgnoringUsernameAsync(mozilla.components.concept.storage.Login)/newLogin)

Note that this method is not thread safe.

**Returns**
a list of potential duplicate [Login](../-login/index.md)s

