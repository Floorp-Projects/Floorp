[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginsStorage](index.md) / [importLoginsAsync](./import-logins-async.md)

# importLoginsAsync

`abstract suspend fun importLoginsAsync(logins: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L99)

Bulk-import of a list of [Login](../-login/index.md).
Storage must be empty; implementations expected to throw otherwise.

### Parameters

`logins` - A list of [Login](../-login/index.md) records to be imported.

**Return**
JSON object with detailed information about imported logins.

