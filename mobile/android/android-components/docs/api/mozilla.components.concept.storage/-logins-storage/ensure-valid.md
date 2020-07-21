[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginsStorage](index.md) / [ensureValid](./ensure-valid.md)

# ensureValid

`abstract suspend fun ensureValid(login: `[`Login`](../-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L106)

Checks if login already exists and is valid. Implementations expected to throw for invalid [login](ensure-valid.md#mozilla.components.concept.storage.LoginsStorage$ensureValid(mozilla.components.concept.storage.Login)/login).

### Parameters

`login` - A [Login](../-login/index.md) record to validate.