[android-components](../../index.md) / [mozilla.components.browser.engine.gecko](../index.md) / [GeckoEngineSession](index.md) / [saveState](./save-state.md)

# saveState

`fun saveState(): `[`EngineSessionState`](../../mozilla.components.concept.engine/-engine-session-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/GeckoEngineSession.kt#L137)

Overrides [EngineSession.saveState](../../mozilla.components.concept.engine/-engine-session/save-state.md)

See [EngineSession.saveState](../../mozilla.components.concept.engine/-engine-session/save-state.md)

See https://bugzilla.mozilla.org/show_bug.cgi?id=1441810 for
discussion on sync vs. async, where a decision was made that
callers should provide synchronous wrappers, if needed. In case we're
asking for the state when persisting, a separate (independent) thread
is used so we're not blocking anything else. In case of calling this
method from onPause or similar, we also want a synchronous response.

