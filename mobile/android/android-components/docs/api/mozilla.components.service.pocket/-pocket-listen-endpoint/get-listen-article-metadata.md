[android-components](../../index.md) / [mozilla.components.service.pocket](../index.md) / [PocketListenEndpoint](index.md) / [getListenArticleMetadata](./get-listen-article-metadata.md)

# getListenArticleMetadata

`@WorkerThread fun getListenArticleMetadata(articleID: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, articleURL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`PocketResponse`](../../mozilla.components.service.pocket.net/-pocket-response/index.md)`<`[`PocketListenArticleMetadata`](../../mozilla.components.service.pocket.data/-pocket-listen-article-metadata/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/PocketListenEndpoint.kt#L33)

Gets a response, filled with the given article's listen metadata on success. This network call is
synchronous and the results are not cached.

**Return**
a [PocketResponse.Success](../../mozilla.components.service.pocket.net/-pocket-response/-success/index.md) with the given article's listen metada on success or, on error, a
[PocketResponse.Failure](../../mozilla.components.service.pocket.net/-pocket-response/-failure.md).

