[android-components](../../index.md) / [mozilla.components.service.glean.net](../index.md) / [PingUploader](./index.md)

# PingUploader

`interface PingUploader` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/net/PingUploader.kt#L12)

The interface defining how to send pings.

### Functions

| Name | Summary |
|---|---|
| [upload](upload.md) | `abstract fun upload(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, headers: `[`HeadersList`](../-headers-list.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Synchronously upload a ping to a server. |

### Inheritors

| Name | Summary |
|---|---|
| [BaseUploader](../-base-uploader/index.md) | `class BaseUploader : `[`PingUploader`](./index.md)<br>The logic for uploading pings: this leaves the actual upload implementation to the user-provided delegate. |
| [ConceptFetchHttpUploader](../-concept-fetch-http-uploader/index.md) | `class ConceptFetchHttpUploader : `[`PingUploader`](./index.md)<br>A simple ping Uploader, which implements a "send once" policy, never storing or attempting to send the ping again. This uses Android Component's `concept-fetch`. |
