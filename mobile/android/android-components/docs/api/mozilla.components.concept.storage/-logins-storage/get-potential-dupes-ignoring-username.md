[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginsStorage](index.md) / [getPotentialDupesIgnoringUsername](./get-potential-dupes-ignoring-username.md)

# getPotentialDupesIgnoringUsername

`abstract suspend fun getPotentialDupesIgnoringUsername(login: `[`Login`](../-login/index.md)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L122)

Fetch the list of potential duplicate logins from the underlying storage layer, ignoring username.

### Parameters

`login` - The [Login](../-login/index.md) to compare against for finding dupes.

**Return**
A list of [Login](../-login/index.md) objects, representing matching potential dupe logins.

