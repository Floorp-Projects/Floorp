[android-components](../../index.md) / [mozilla.components.service.glean.net](../index.md) / [ConceptFetchHttpUploader](index.md) / [upload](./upload.md)

# upload

`fun upload(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, headers: `[`HeadersList`](../-headers-list.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/net/ConceptFetchHttpUploader.kt#L48)

Overrides [PingUploader.upload](../-ping-uploader/upload.md)

Synchronously upload a ping to a server.

### Parameters

`url` - the URL path to upload the data to

`data` - the serialized text data to send

`headers` - a [HeadersList](../-headers-list.md) containing String to String [Pair](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html) with
    the first entry being the header name and the second its value.

**Return**
true if the ping was correctly dealt with (sent successfully
    or faced an unrecoverable error), false if there was a recoverable
    error callers can deal with.

