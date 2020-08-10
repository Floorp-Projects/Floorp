[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginsStorage](index.md) / [getByBaseDomain](./get-by-base-domain.md)

# getByBaseDomain

`abstract suspend fun getByBaseDomain(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L114)

Fetch the list of logins for some origin from the underlying storage layer.

### Parameters

`origin` - A host name used to look up [Login](../-login/index.md) records.

**Return**
A list of [Login](../-login/index.md) objects, representing matching logins.

