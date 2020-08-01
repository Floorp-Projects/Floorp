[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [call](./call.md)

# call

`fun <ERROR CLASS>.call(phoneNumber: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, subject: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = getString(R.string.mozac_support_ktx_share_dialog_title)): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/Context.kt#L149)

Calls phone number via [ACTION_DIAL](#) intent.

Note: we purposely use ACTION_DIAL rather than ACTION_CALL as the latter requires user permission

### Parameters

`phoneNumber` - the phone number to send to [ACTION_DIAL](#)

`subject` - of the intent [EXTRA_TEXT](#)

**Return**
true it is able to share phone call false otherwise.

