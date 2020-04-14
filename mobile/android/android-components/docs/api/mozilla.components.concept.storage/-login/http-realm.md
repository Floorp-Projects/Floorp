[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [Login](index.md) / [httpRealm](./http-realm.md)

# httpRealm

`val httpRealm: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L152)

The HTTP realm this login entry was requested for.
This only applies to non-form-based login entries.
It's derived from the WWW-Authenticate header set in a HTTP 401
response, see RFC2617 for details.

