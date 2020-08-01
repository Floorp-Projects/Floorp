[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketListenEndpoint](./index.md)

# PocketListenEndpoint

`class PocketListenEndpoint` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketListenEndpoint.kt#L20)

Makes requests to the Pocket Listen API and returns the requested data.

**See Also**

[newInstance](new-instance.md)

### Functions

| Name | Summary |
|---|---|
| [getListenArticleMetadata](get-listen-article-metadata.md) | `fun getListenArticleMetadata(articleID: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, articleURL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`PocketResponse`](../../mozilla.components.service.pocket.net/-pocket-response/index.md)`<`[`PocketListenArticleMetadata`](../../mozilla.components.service.pocket.data/-pocket-listen-article-metadata/index.md)`>`<br>Gets a response, filled with the given article's listen metadata on success. This network call is synchronous and the results are not cached. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [newInstance](new-instance.md) | `fun newInstance(client: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`, accessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, userAgent: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`PocketListenEndpoint`](./index.md)<br>Returns a new instance of [PocketListenEndpoint](./index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
