[android-components](../../index.md) / [mozilla.components.service.fxa.sharing](../index.md) / [AccountSharing](./index.md)

# AccountSharing

`object AccountSharing` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sharing/AccountSharing.kt#L39)

Allows querying trusted FxA Auth provider packages on the device for instances of [ShareableAccount](../-shareable-account/index.md).
Once an instance of [ShareableAccount](../-shareable-account/index.md) is obtained, it may be used with
[FxaAccountManager.migrateAccountAsync](#) directly,
or with [FirefoxAccount.migrateFromSessionTokenAsync](#) via [ShareableAccount.authInfo](../-shareable-account/auth-info.md).

### Functions

| Name | Summary |
|---|---|
| [getSignaturePostAPI28](get-signature-post-a-p-i28.md) | `fun getSignaturePostAPI28(packageManager: <ERROR CLASS>, packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Obtains package signature on devices running API&gt;=28. Takes into consideration multiple signers and certificate rotation. |
| [getSignaturePreAPI28](get-signature-pre-a-p-i28.md) | `fun getSignaturePreAPI28(packageManager: <ERROR CLASS>, packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Obtains package signature on devices running API&lt;28. Takes into consideration multiple signers, but not signature rotation. |
| [packageExistsWithSignature](package-exists-with-signature.md) | `fun packageExistsWithSignature(packageManager: <ERROR CLASS>, suspectPackage: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, expectedSignature: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if package exists, and that its signature matches provided value. |
| [queryShareableAccounts](query-shareable-accounts.md) | `fun queryShareableAccounts(context: <ERROR CLASS>): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ShareableAccount`](../-shareable-account/index.md)`>`<br>Queries trusted FxA Auth providers present on the device, returning a list of accounts that can be used for signing in automatically. |
