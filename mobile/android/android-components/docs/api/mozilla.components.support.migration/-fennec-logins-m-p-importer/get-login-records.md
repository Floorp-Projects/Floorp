[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecLoginsMPImporter](index.md) / [getLoginRecords](./get-login-records.md)

# getLoginRecords

`fun getLoginRecords(password: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, crashReporter: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ServerPassword`](../../mozilla.components.service.sync.logins/-server-password.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecLoginsMPImporter.kt#L58)

### Parameters

`password` - An MP to use for decrypting the Fennec logins database.

`crashReporter` - [CrashReporting](../../mozilla.components.support.base.crash/-crash-reporting/index.md) instance for recording encountered exceptions.

**Return**
A list of [ServerPassword](../../mozilla.components.service.sync.logins/-server-password.md) records representing logins stored in Fennec.

