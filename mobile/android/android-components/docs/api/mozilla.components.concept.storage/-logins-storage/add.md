[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginsStorage](index.md) / [add](./add.md)

# add

`abstract suspend fun add(login: `[`Login`](../-login/index.md)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L74)

Inserts the provided login into the database, returning it's id.

This function ignores values in metadata fields (`timesUsed`,
`timeCreated`, `timeLastUsed`, and `timePasswordChanged`).

If login has an empty id field, then a GUID will be
generated automatically. The format of generated guids
are left up to the implementation of LoginsStorage (in
practice the [DatabaseLoginsStorage](#) generates 12-character
base64url (RFC 4648) encoded strings.

This will return an error result if a GUID is provided but
collides with an existing record, or if the provided record
is invalid (missing password, origin, or doesn't have exactly
one of formSubmitURL and httpRealm).

### Parameters

`login` - A [Login](../-login/index.md) record to add.

**Return**
A `guid` for the created record.

