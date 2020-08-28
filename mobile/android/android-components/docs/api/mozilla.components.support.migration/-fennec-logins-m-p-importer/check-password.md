[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecLoginsMPImporter](index.md) / [checkPassword](./check-password.md)

# checkPassword

`fun checkPassword(password: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecLoginsMPImporter.kt#L43)

Checks if provided [password](check-password.md#mozilla.components.support.migration.FennecLoginsMPImporter$checkPassword(kotlin.String)/password) is able to unlock MP-protected logins storage.

### Parameters

`password` - Password to check.

**Return**
'true' if [password](check-password.md#mozilla.components.support.migration.FennecLoginsMPImporter$checkPassword(kotlin.String)/password) is correct, 'false otherwise (and in case of any errors).

