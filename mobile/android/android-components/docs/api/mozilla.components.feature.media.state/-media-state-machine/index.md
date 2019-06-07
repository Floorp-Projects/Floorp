[android-components](../../index.md) / [mozilla.components.feature.media.state](../index.md) / [MediaStateMachine](./index.md)

# MediaStateMachine

`class MediaStateMachine : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/state/MediaStateMachine.kt#L21)

A state machine that subscribes to all [Session](../../mozilla.components.browser.session/-session/index.md) instances and watches changes to their [Media](../../mozilla.components.concept.engine.media/-media/index.md) to create an
aggregated [MediaState](../-media-state/index.md).

Other components can subscribe to the state machine to get notified about [MediaState](../-media-state/index.md) changes.

### Types

| Name | Summary |
|---|---|
| [Observer](-observer/index.md) | `interface Observer`<br>Interface for observers that are interested in [MediaState](../-media-state/index.md) changes. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MediaStateMachine(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`> = ObserverRegistry())`<br>A state machine that subscribes to all [Session](../../mozilla.components.browser.session/-session/index.md) instances and watches changes to their [Media](../../mozilla.components.concept.engine.media/-media/index.md) to create an aggregated [MediaState](../-media-state/index.md). |

### Functions

| Name | Summary |
|---|---|
| [getState](get-state.md) | `fun getState(): `[`MediaState`](../-media-state/index.md)<br>Get the current [MediaState](../-media-state/index.md). |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Start observing [Session](../../mozilla.components.browser.session/-session/index.md) and their [Media](../../mozilla.components.concept.engine.media/-media/index.md) and create an aggregated [MediaState](../-media-state/index.md) that can be observed. |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Stop observing [Session](../../mozilla.components.browser.session/-session/index.md) and their [Media](../../mozilla.components.concept.engine.media/-media/index.md). |
