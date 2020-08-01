[android-components](../../index.md) / [mozilla.components.service.glean.net](../index.md) / [ConceptFetchHttpUploader](./index.md)

# ConceptFetchHttpUploader

`class ConceptFetchHttpUploader : `[`PingUploader`](../-ping-uploader.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/net/ConceptFetchHttpUploader.kt#L29)

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
| [upload](upload.md) | `fun upload(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, headers: HeadersList): UploadResult`<br>Synchronously upload a ping to a server. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [DEFAULT_CONNECTION_TIMEOUT](-d-e-f-a-u-l-t_-c-o-n-n-e-c-t-i-o-n_-t-i-m-e-o-u-t.md) | `const val DEFAULT_CONNECTION_TIMEOUT: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
| [DEFAULT_READ_TIMEOUT](-d-e-f-a-u-l-t_-r-e-a-d_-t-i-m-e-o-u-t.md) | `const val DEFAULT_READ_TIMEOUT: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromClient](from-client.md) | `fun fromClient(client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`): `[`ConceptFetchHttpUploader`](./index.md)<br>Export a constructor that is usable from Java. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
