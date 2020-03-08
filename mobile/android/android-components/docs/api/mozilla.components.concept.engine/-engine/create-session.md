[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [createSession](./create-session.md)

# createSession

`@MainThread abstract fun createSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`EngineSession`](../-engine-session/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L90)

Creates a new engine session. If [speculativeCreateSession](speculative-create-session.md) is supported this
method returns the prepared [EngineSession](../-engine-session/index.md) if it is still applicable i.e.
the parameter(s) ([private](create-session.md#mozilla.components.concept.engine.Engine$createSession(kotlin.Boolean)/private)) are equal.

### Parameters

`private` - whether or not this session should use private mode.

**Return**
the newly created [EngineSession](../-engine-session/index.md).

