[android-components](../../index.md) / [mozilla.components.browser.session.state](../index.md) / [DownloadState](./index.md)

# DownloadState

`data class DownloadState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/state/DownloadState.kt#L19)

Value type that represents a Download.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadState(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, contentLength: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, destinationDirectory: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = Environment.DIRECTORY_DOWNLOADS)`<br>Value type that represents a Download. |

### Properties

| Name | Summary |
|---|---|
| [contentLength](content-length.md) | `val contentLength: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>The file size reported by the server. |
| [contentType](content-type.md) | `val contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Content type (MIME type) to indicate the media type of the download. |
| [destinationDirectory](destination-directory.md) | `val destinationDirectory: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The matching destination directory for this type of download. |
| [fileName](file-name.md) | `val fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>A canonical filename for this download. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The full url to the content that should be downloaded. |
| [userAgent](user-agent.md) | `val userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The user agent to be used for the download. |
