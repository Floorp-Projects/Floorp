[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [DownloadUtils](index.md) / [guessFileName](./guess-file-name.md)

# guessFileName

`@JvmStatic fun guessFileName(contentDisposition: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/DownloadUtils.kt#L63)

Guess the name of the file that should be downloaded.

This method is largely identical to [android.webkit.URLUtil.guessFileName](https://developer.android.com/reference/android/webkit/URLUtil.html#guessFileName(java.lang.String, java.lang.String, java.lang.String))
which unfortunately does not implement RfC 5987.

