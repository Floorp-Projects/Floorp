[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [DefaultLoginValidationDelegate](index.md) / [mergeWithLogin](./merge-with-login.md)

# mergeWithLogin

`fun `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`.mergeWithLogin(login: `[`Login`](../../mozilla.components.concept.storage/-login/index.md)`): `[`Login`](../../mozilla.components.concept.storage/-login/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/DefaultLoginValidationDelegate.kt#L104)

Will use values from [login](merge-with-login.md#mozilla.components.service.sync.logins.DefaultLoginValidationDelegate.Companion$mergeWithLogin(mozilla.components.concept.storage.Login, mozilla.components.concept.storage.Login)/login) if they are 1) non-null and 2) non-empty.  Otherwise, will fall
back to values from [this](merge-with-login/-this-.md).

