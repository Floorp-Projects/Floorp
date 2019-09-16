[android-components](../../index.md) / [mozilla.components.service.glean.net](../index.md) / [ConceptFetchHttpUploader](./index.md)

# ConceptFetchHttpUploader

`class ConceptFetchHttpUploader : `[`PingUploader`](../-ping-uploader/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/net/ConceptFetchHttpUploader.kt#L24)

A simple ping Uploader, which implements a "send once" policy, never
storing or attempting to send the ping again. This uses Android Component's
`concept-fetch`.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ConceptFetchHttpUploader(client: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`>)`<br>A simple ping Uploader, which implements a "send once" policy, never storing or attempting to send the ping again. This uses Android Component's `concept-fetch`. |

### Functions

| Name | Summary |
|---|---|
| [upload](upload.md) | `fun upload(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, headers: `[`HeadersList`](../-headers-list.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Synchronously upload a ping to a server. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [DEFAULT_CONNECTION_TIMEOUT](-d-e-f-a-u-l-t_-c-o-n-n-e-c-t-i-o-n_-t-i-m-e-o-u-t.md) | `const val DEFAULT_CONNECTION_TIMEOUT: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [DEFAULT_READ_TIMEOUT](-d-e-f-a-u-l-t_-r-e-a-d_-t-i-m-e-o-u-t.md) | `const val DEFAULT_READ_TIMEOUT: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
