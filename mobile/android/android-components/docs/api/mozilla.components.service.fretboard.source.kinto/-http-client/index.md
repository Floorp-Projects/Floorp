[android-components](../../index.md) / [mozilla.components.service.fretboard.source.kinto](../index.md) / [HttpClient](./index.md)

# HttpClient

`interface HttpClient` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/fretboard/src/main/java/mozilla/components/service/fretboard/source/kinto/HttpClient.kt#L14)

Represents an http client, used to
make it easy to swap implementations
as needed

### Functions

| Name | Summary |
|---|---|
| [get](get.md) | `abstract fun get(url: `[`URL`](https://developer.android.com/reference/java/net/URL.html)`, headers: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Performs a GET request to the specified URL, supplying the provided headers |
