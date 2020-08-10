[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [email](./email.md)

# email

`fun <ERROR CLASS>.email(address: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, subject: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = getString(R.string.mozac_support_ktx_share_dialog_title)): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/Context.kt#L124)

Emails content via [ACTION_SENDTO](#) intent.

### Parameters

`address` - the email address to send to [EXTRA_EMAIL](#)

`subject` - of the intent [EXTRA_TEXT](#)

**Return**
true it is able to share email false otherwise.

