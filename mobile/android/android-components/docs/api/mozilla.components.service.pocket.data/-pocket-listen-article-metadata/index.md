[android-components](../../index.md) / [mozilla.components.service.pocket.data](../index.md) / [PocketListenArticleMetadata](./index.md)

# PocketListenArticleMetadata

`data class PocketListenArticleMetadata` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/data/PocketListenArticleMetadata.kt#L26)

The metadata for a spoken article's audio file.

Some documentation on these values, directly from the endpoint, can be found here:
https://documenter.getpostman.com/view/777613/S17jXChA#426b37e2-0a4f-bd65-35c6-e14f1b19d0f0

### Types

| Name | Summary |
|---|---|
| [Status](-status/index.md) | `enum class Status`<br>A status representing whether or not the audio file is available. |

### Properties

| Name | Summary |
|---|---|
| [audioUrl](audio-url.md) | `val audioUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the url to the spoken article's audio file. |
| [durationSeconds](duration-seconds.md) | `val durationSeconds: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>length of the audio in seconds. |
| [format](format.md) | `val format: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the encoding format of the audio file, e.g. "mp3". |
| [size](size.md) | `val size: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>size of the audio file in bytes. |
| [status](status.md) | `val status: `[`Status`](-status/index.md)<br>whether or not the audio file is available. |
| [voice](voice.md) | `val voice: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the voice name used to speak the article content, e.g. "Salli". |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
