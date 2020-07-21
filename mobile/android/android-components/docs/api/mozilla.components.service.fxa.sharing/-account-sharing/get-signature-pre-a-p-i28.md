[android-components](../../index.md) / [mozilla.components.service.fxa.sharing](../index.md) / [AccountSharing](index.md) / [getSignaturePreAPI28](./get-signature-pre-a-p-i28.md)

# getSignaturePreAPI28

`fun getSignaturePreAPI28(packageManager: <ERROR CLASS>, packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sharing/AccountSharing.kt#L183)

Obtains package signature on devices running API&lt;28. Takes into consideration multiple signers,
but not signature rotation.

**Return**
A certificate SHA256 fingerprint, if one could be reliably obtained.

