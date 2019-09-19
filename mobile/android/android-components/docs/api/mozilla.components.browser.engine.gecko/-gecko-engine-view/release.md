[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngineView](index.md) / [release](./release.md)

# release

`@Synchronized fun release(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngineView.kt#L99)

Overrides [EngineView.release](../../mozilla.components.concept.engine/-engine-view/release.md)

Releases an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) that is currently rendered by this view (after calling [render](../../mozilla.components.concept.engine/-engine-view/render.md)).

Usually an app does not need to call this itself since [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) will take care of that if it gets detached.
However there are situations where an app wants to hand-off rendering of an [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) to a different
[EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) without the current [EngineView](../../mozilla.components.concept.engine/-engine-view/index.md) getting detached immediately.

