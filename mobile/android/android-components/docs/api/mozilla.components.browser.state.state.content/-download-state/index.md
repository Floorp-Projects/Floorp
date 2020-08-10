[android-components](../../index.md) / [mozilla.components.browser.state.state.content](../index.md) / [DownloadState](./index.md)

# DownloadState

`data class DownloadState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/content/DownloadState.kt#L32)

Value type that represents a download request.

### Types

| Name | Summary |
|---|---|
| [Status](-status/index.md) | `enum class Status`<br>Status that represents every state that a download can be in. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DownloadState(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, contentLength: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`? = null, currentBytesCopied: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0, status: `[`Status`](-status/index.md)` = Status.INITIATED, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, destinationDirectory: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = Environment.DIRECTORY_DOWNLOADS, referrerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, skipConfirmation: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, id: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = Random.nextLong(), sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Value type that represents a download request. |

### Properties

| Name | Summary |
|---|---|
| [contentLength](content-length.md) | `val contentLength: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?`<br>The file size reported by the server. |
| [contentType](content-type.md) | `val contentType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Content type (MIME type) to indicate the media type of the download. |
| [currentBytesCopied](current-bytes-copied.md) | `val currentBytesCopied: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>The number of current bytes copied. |
| [destinationDirectory](destination-directory.md) | `val destinationDirectory: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The matching destination directory for this type of download. |
| [fileName](file-name.md) | `val fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>A canonical filename for this download. |
| [filePath](file-path.md) | `val filePath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The file path the file was saved at. |
| [id](id.md) | `val id: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>The unique identifier of this download. |
| [referrerUrl](referrer-url.md) | `val referrerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The site that linked to this download. |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Identifier of the session that spawned the download. @ |
| [skipConfirmation](skip-confirmation.md) | `val skipConfirmation: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not the confirmation dialog should be shown before the download begins. |
| [status](status.md) | `val status: `[`Status`](-status/index.md)<br>The current status of the download. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The full url to the content that should be downloaded. |
| [userAgent](user-agent.md) | `val userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The user agent to be used for the download. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
