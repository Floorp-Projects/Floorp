[android-components](../../index.md) / [mozilla.components.concept.engine.content.blocking](../index.md) / [TrackingProtectionExceptionStorage](index.md) / [removeAll](./remove-all.md)

# removeAll

`abstract fun removeAll(activeSessions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/content/blocking/TrackingProtectionExceptionStorage.kt#L53)

Removes all domains from the exception list.

### Parameters

`activeSessions` - A list of all active sessions (including CustomTab
sessions) to be notified.