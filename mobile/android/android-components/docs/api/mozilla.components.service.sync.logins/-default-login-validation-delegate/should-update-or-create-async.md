[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [DefaultLoginValidationDelegate](index.md) / [shouldUpdateOrCreateAsync](./should-update-or-create-async.md)

# shouldUpdateOrCreateAsync

`fun shouldUpdateOrCreateAsync(newLogin: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`, potentialDupes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../../mozilla.components.concept.storage/-login/index.md)`>?): Deferred<`[`Result`](../../mozilla.components.concept.storage/-login-validation-delegate/-result/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/DefaultLoginValidationDelegate.kt#L31)

Overrides [LoginValidationDelegate.shouldUpdateOrCreateAsync](../../mozilla.components.concept.storage/-login-validation-delegate/should-update-or-create-async.md)

Compares a [Login](../../mozilla.components.concept.storage/-login/index.md) to a passed in list of potential dupes [Login](../../mozilla.components.concept.storage/-login/index.md) or queries underlying
storage for potential dupes list of [Login](../../mozilla.components.concept.storage/-login/index.md) to determine if it should be updated or created.

