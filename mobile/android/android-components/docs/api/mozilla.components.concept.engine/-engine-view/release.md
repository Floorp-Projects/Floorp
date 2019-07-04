[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [EngineView](index.md) / [release](./release.md)

# release

`abstract fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineView.kt#L35)

Releases an [EngineSession](../-engine-session/index.md) that is currently rendered by this view (after calling [render](render.md)).

Usually an app does not need to call this itself since [EngineView](index.md) will take care of that if it gets detached.
However there are situations where an app wants to hand-off rendering of an [EngineSession](../-engine-session/index.md) to a different
[EngineView](index.md) without the current [EngineView](index.md) getting detached immediately.

