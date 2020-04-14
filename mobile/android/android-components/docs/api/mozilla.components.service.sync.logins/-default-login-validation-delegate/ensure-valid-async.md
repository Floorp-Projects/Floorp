[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [DefaultLoginValidationDelegate](index.md) / [ensureValidAsync](./ensure-valid-async.md)

# ensureValidAsync

`fun ensureValidAsync(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): Deferred<`[`Result`](../../mozilla.components.concept.storage/-login-validation-delegate/-result/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/DefaultLoginValidationDelegate.kt#L70)

Queries underlying storage for a new [Login](../../mozilla.components.concept.storage/-login/index.md) to determine if should be updated or created,
and can be merged and saved, or any error states.

**Return**
a [Result](../../mozilla.components.concept.storage/-login-validation-delegate/-result/index.md) detailing whether we should create or update (and the merged login to update),
else any error states

