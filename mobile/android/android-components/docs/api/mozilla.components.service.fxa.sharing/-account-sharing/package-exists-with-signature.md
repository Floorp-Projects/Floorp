[android-components](../../index.md) / [mozilla.components.service.fxa.sharing](../index.md) / [AccountSharing](index.md) / [packageExistsWithSignature](./package-exists-with-signature.md)

# packageExistsWithSignature

`fun packageExistsWithSignature(packageManager: <ERROR CLASS>, suspectPackage: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, expectedSignature: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sharing/AccountSharing.kt#L123)

Checks if package exists, and that its signature matches provided value.

### Parameters

`packageManager` - [PackageManager](#) used for running checks against [suspectPackage](package-exists-with-signature.md#mozilla.components.service.fxa.sharing.AccountSharing$packageExistsWithSignature(, kotlin.String, kotlin.String)/suspectPackage).

`suspectPackage` - Package name to check.

`expectedSignature` - Expected signature of the [suspectPackage](package-exists-with-signature.md#mozilla.components.service.fxa.sharing.AccountSharing$packageExistsWithSignature(, kotlin.String, kotlin.String)/suspectPackage).

**Return**
Boolean flag, true if package is present and its signature matches [expectedSignature](package-exists-with-signature.md#mozilla.components.service.fxa.sharing.AccountSharing$packageExistsWithSignature(, kotlin.String, kotlin.String)/expectedSignature).

