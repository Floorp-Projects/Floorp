[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginsStorage](index.md) / [get](./get.md)

# get

`abstract suspend fun get(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Login`](../-login/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L38)

Fetches a password from the underlying storage layer by its unique identifier.

### Parameters

`guid` - Unique identifier for the desired record.

**Return**
[Login](../-login/index.md) record, or `null` if the record does not exist.

