[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [EngineState](index.md) / [engineObserver](./engine-observer.md)

# engineObserver

`val engineObserver: `[`Observer`](../../mozilla.components.concept.engine/-engine-session/-observer/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/EngineState.kt#L24)

the [EngineSession.Observer](../../mozilla.components.concept.engine/-engine-session/-observer/index.md) linked to [engineSession](engine-session.md). It is
used to observe engine events and update the store. It should become obsolete, once the
migration to browser state is complete, as the engine will then have direct access to
the store.

### Property

`engineObserver` - the [EngineSession.Observer](../../mozilla.components.concept.engine/-engine-session/-observer/index.md) linked to [engineSession](engine-session.md). It is
used to observe engine events and update the store. It should become obsolete, once the
migration to browser state is complete, as the engine will then have direct access to
the store.