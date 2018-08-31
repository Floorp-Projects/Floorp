---
title: HttpClient - 
---

[mozilla.components.service.fretboard.source.kinto](../index.html) / [HttpClient](./index.html)

# HttpClient

`interface HttpClient`

Represents an http client, used to
make it easy to swap implementations
as needed

### Functions

| [get](get.html) | `abstract fun get(url: `[`URL`](http://docs.oracle.com/javase/6/docs/api/java/net/URL.html)`, headers: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Performs a GET request to the specified URL, supplying the provided headers |

