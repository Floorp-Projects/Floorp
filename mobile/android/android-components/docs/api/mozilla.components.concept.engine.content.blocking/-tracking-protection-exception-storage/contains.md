[android-components](../../index.md) / [mozilla.components.concept.engine.content.blocking](../index.md) / [TrackingProtectionExceptionStorage](index.md) / [contains](./contains.md)

# contains

`abstract fun contains(session: `[`EngineSession`](../../mozilla.components.concept.engine/-engine-session/index.md)`, onResult: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/content/blocking/TrackingProtectionExceptionStorage.kt#L46)

Indicates if a given [session](contains.md#mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage$contains(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/session) is in the exception list.

### Parameters

`session` - The [session](contains.md#mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage$contains(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/session) to be verified.

`onResult` - A callback to inform if the given [session](contains.md#mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage$contains(mozilla.components.concept.engine.EngineSession, kotlin.Function1((kotlin.Boolean, kotlin.Unit)))/session) is in
the exception list, true if it is in, otherwise false.