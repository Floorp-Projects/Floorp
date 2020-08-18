[android-components](../index.md) / [mozilla.components.support.ktx.android.net](index.md) / [isInScope](./is-in-scope.md)

# isInScope

`fun <ERROR CLASS>.isInScope(scopes: `[`Iterable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-iterable/index.html)`<<ERROR CLASS>>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/net/Uri.kt#L48)

Checks that the given URL is in one of the given URL [scopes](is-in-scope.md#mozilla.components.support.ktx.android.net$isInScope(, kotlin.collections.Iterable(()))/scopes).

https://www.w3.org/TR/appmanifest/#dfn-within-scope

### Parameters

`scopes` - Uris that each represent a scope.
A Uri is within the scope if the origin matches and it starts with the scope's path.

**Return**
True if this Uri is within any of the given scopes.

