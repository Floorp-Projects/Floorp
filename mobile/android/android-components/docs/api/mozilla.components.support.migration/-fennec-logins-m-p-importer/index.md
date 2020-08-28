[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecLoginsMPImporter](./index.md)

# FennecLoginsMPImporter

`class FennecLoginsMPImporter` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecLoginsMPImporter.kt#L16)

Helper class that allows:

* checking for presence of MP on the Fennec's logins database
* reading MP-protected logins from Fennec's logins database

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FennecLoginsMPImporter(profile: `[`FennecProfile`](../-fennec-profile/index.md)`)`<br>Helper class that allows: |

### Functions

| Name | Summary |
|---|---|
| [checkPassword](check-password.md) | `fun checkPassword(password: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if provided [password](check-password.md#mozilla.components.support.migration.FennecLoginsMPImporter$checkPassword(kotlin.String)/password) is able to unlock MP-protected logins storage. |
| [getLoginRecords](get-login-records.md) | `fun getLoginRecords(password: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, crashReporter: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ServerPassword`](../../mozilla.components.service.sync.logins/-server-password.md)`>` |
| [hasMasterPassword](has-master-password.md) | `fun hasMasterPassword(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
