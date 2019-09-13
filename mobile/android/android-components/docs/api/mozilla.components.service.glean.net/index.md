[android-components](../index.md) / [mozilla.components.service.glean.net](./index.md)

## Package mozilla.components.service.glean.net

### Types

| Name | Summary |
|---|---|
| [BaseUploader](-base-uploader/index.md) | `class BaseUploader : `[`PingUploader`](-ping-uploader/index.md)<br>The logic for uploading pings: this leaves the actual upload implementation to the user-provided delegate. |
| [ConceptFetchHttpUploader](-concept-fetch-http-uploader/index.md) | `class ConceptFetchHttpUploader : `[`PingUploader`](-ping-uploader/index.md)<br>A simple ping Uploader, which implements a "send once" policy, never storing or attempting to send the ping again. This uses Android Component's `concept-fetch`. |
| [PingUploader](-ping-uploader/index.md) | `interface PingUploader`<br>The interface defining how to send pings. |

### Type Aliases

| Name | Summary |
|---|---|
| [HeadersList](-headers-list.md) | `typealias HeadersList = `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Pair`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>>` |
