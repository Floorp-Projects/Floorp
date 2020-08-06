[android-components](../index.md) / [mozilla.components.support.ktx.android.content](index.md) / [share](./share.md)

# share

`fun <ERROR CLASS>.share(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, subject: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = getString(R.string.mozac_support_ktx_share_dialog_title)): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/content/Context.kt#L90)

Shares content via [ACTION_SEND](#) intent.

### Parameters

`text` - the data to be shared [EXTRA_TEXT](#)

`subject` - of the intent [EXTRA_TEXT](#)

**Return**
true it is able to share false otherwise.

