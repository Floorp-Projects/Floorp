[android-components](../../index.md) / [mozilla.components.service.glean.net](../index.md) / [ConceptFetchHttpUploader](index.md) / [upload](./upload.md)

# upload

`fun upload(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, data: `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`, headers: HeadersList): UploadResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/net/ConceptFetchHttpUploader.kt#L63)

Synchronously upload a ping to a server.

### Parameters

`url` - the URL path to upload the data to

`data` - the serialized text data to send

`headers` - a [HeadersList](#) containing String to String [Pair](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-pair/index.html) with
    the first entry being the header name and the second its value.

**Return**
true if the ping was correctly dealt with (sent successfully
    or faced an unrecoverable error), false if there was a recoverable
    error callers can deal with.

