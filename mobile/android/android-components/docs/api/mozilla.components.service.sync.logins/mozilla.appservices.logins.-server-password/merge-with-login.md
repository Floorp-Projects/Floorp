[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [mozilla.appservices.logins.ServerPassword](index.md) / [mergeWithLogin](./merge-with-login.md)

# mergeWithLogin

`fun `[`ServerPassword`](../-server-password.md)`.mergeWithLogin(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`ServerPassword`](../-server-password.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/GeckoLoginStorageDelegate.kt#L135)

Will use values from [this](merge-with-login/-this-.md) if they are 1) non-null and 2) non-empty.  Otherwise, will fall
back to values from [this](merge-with-login/-this-.md).

