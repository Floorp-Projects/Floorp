[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [LoginValidationDelegate](index.md) / [shouldUpdateOrCreateAsync](./should-update-or-create-async.md)

# shouldUpdateOrCreateAsync

`abstract fun shouldUpdateOrCreateAsync(newLogin: `[`Login`](../-login/index.md)`, potentialDupes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Login`](../-login/index.md)`>? = null): Deferred<`[`Result`](-result/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L249)

Checks whether a [login](#) should be saved or updated.

Note that this method is not thread safe.

**Returns**
a [Result](-result/index.md), detailing whether a [login](#) should be saved or updated.

