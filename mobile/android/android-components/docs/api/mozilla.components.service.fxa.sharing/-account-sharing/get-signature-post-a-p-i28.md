[android-components](../../index.md) / [mozilla.components.service.fxa.sharing](../index.md) / [AccountSharing](index.md) / [getSignaturePostAPI28](./get-signature-post-a-p-i28.md)

# getSignaturePostAPI28

`fun getSignaturePostAPI28(packageManager: <ERROR CLASS>, packageName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sharing/AccountSharing.kt#L144)

Obtains package signature on devices running API&gt;=28. Takes into consideration multiple signers
and certificate rotation.

**Return**
A certificate SHA256 fingerprint, if one could be reliably obtained.

