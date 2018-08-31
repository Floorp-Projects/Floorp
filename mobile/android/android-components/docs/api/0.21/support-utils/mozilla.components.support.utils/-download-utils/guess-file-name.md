---
title: DownloadUtils.guessFileName - 
---

[mozilla.components.support.utils](../index.html) / [DownloadUtils](index.html) / [guessFileName](./guess-file-name.html)

# guessFileName

`@JvmStatic fun guessFileName(contentDisposition: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)

Guess the name of the file that should be downloaded.

This method is largely identical to [android.webkit.URLUtil.guessFileName](#)
which unfortunately does not implement RfC 5987.

