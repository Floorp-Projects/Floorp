[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [Engine](index.md) / [speculativeCreateSession](./speculative-create-session.md)

# speculativeCreateSession

`@MainThread open fun speculativeCreateSession(private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Engine.kt#L129)

Informs the engine that an [EngineSession](../-engine-session/index.md) is likely to be requested soon
via [createSession](create-session.md). This is useful in case creating an engine session is
costly and an application wants to decide when the session should be created
without having to manage the session itself i.e. when it may or may not
need it.

### Parameters

`private` - whether or not the session should use private mode.

`contextId` - the session context ID for the session.