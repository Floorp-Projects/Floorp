[android-components](../../index.md) / [mozilla.components.concept.engine.content.blocking](../index.md) / [TrackingProtectionExceptionStorage](index.md) / [fetchAll](./fetch-all.md)

# fetchAll

`abstract fun fetchAll(onResult: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/content/blocking/TrackingProtectionExceptionStorage.kt#L20)

Fetch all domains that will be ignored for tracking protection.

### Parameters

`onResult` - A callback to inform that the domains in the exception list has been fetched,
it provides a list of all the domains that are on the exception list, if there are none
domains in the exception list, an empty list will be provided.