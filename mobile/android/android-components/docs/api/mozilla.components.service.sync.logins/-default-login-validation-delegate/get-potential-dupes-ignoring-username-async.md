[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [DefaultLoginValidationDelegate](index.md) / [getPotentialDupesIgnoringUsernameAsync](./get-potential-dupes-ignoring-username-async.md)

# getPotentialDupesIgnoringUsernameAsync

`fun getPotentialDupesIgnoringUsernameAsync(newLogin: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): Deferred<`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/DefaultLoginValidationDelegate.kt#L56)

Overrides [LoginValidationDelegate.getPotentialDupesIgnoringUsernameAsync](../../mozilla.components.concept.storage/-login-validation-delegate/get-potential-dupes-ignoring-username-async.md)

Calls underlying storage methods to fetch list of [Login](../../mozilla.components.concept.storage/-login/index.md)s that could be potential dupes of [newLogin](../../mozilla.components.concept.storage/-login-validation-delegate/get-potential-dupes-ignoring-username-async.md#mozilla.components.concept.storage.LoginValidationDelegate$getPotentialDupesIgnoringUsernameAsync(mozilla.components.concept.storage.Login)/newLogin)

Note that this method is not thread safe.

**Returns**
a list of potential duplicate [Login](../../mozilla.components.concept.storage/-login/index.md)s

