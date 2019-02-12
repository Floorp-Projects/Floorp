[android-components](../../index.md) / [mozilla.components.service.fretboard.source.kinto](../index.md) / [HttpClient](index.md) / [get](./get.md)

# get

`abstract fun get(url: `[`URL`](https://developer.android.com/reference/java/net/URL.html)`, headers: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/source/kinto/HttpClient.kt#L24)

Performs a GET request to the specified URL, supplying
the provided headers

### Parameters

`url` - destination url

`headers` - headers to submit with the request

**Return**
HTTP response

