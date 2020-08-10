[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginsStorage](index.md) / [update](./update.md)

# update

`abstract suspend fun update(login: `[`Login`](../-login/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L90)

Updates the fields in the provided record.

This will throw if `login.id` does not refer to
a record that exists in the database, or if the provided record
is invalid (missing password, origin, or doesn't have exactly
one of formSubmitURL and httpRealm).

Like `add`, this function will ignore values in metadata
fields (`timesUsed`, `timeCreated`, `timeLastUsed`, and
`timePasswordChanged`).

### Parameters

`login` - A [Login](../-login/index.md) record instance to update.