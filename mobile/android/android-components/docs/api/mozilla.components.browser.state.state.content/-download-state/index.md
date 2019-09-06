[android-components](../../index.md) / [mozilla.components.browser.state.state.content](../index.md) / [DownloadState](./index.md)

# DownloadState

`data class DownloadState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/content/DownloadState.kt#L21)

Value type that represents a download request.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadState(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, contentLength: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, destinationDirectory: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = Environment.DIRECTORY_DOWNLOADS, referrerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString())`<br>Value type that represents a download request. |

### Properties

| Name | Summary |
|---|---|
| [contentLength](content-length.md) | `val contentLength: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>The file size reported by the server. |
| [contentType](content-type.md) | `val contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Content type (MIME type) to indicate the media type of the download. |
| [destinationDirectory](destination-directory.md) | `val destinationDirectory: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The matching destination directory for this type of download. |
| [fileName](file-name.md) | `val fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>A canonical filename for this download. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [referrerUrl](referrer-url.md) | `val referrerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The site that linked to this download. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The full url to the content that should be downloaded. |
| [userAgent](user-agent.md) | `val userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The user agent to be used for the download. |
