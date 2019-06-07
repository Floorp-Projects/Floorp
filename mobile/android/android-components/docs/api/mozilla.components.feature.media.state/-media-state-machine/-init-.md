[android-components](../../index.md) / [mozilla.components.feature.media.state](../index.md) / [MediaStateMachine](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`MediaStateMachine(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, delegate: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`> = ObserverRegistry())`

A state machine that subscribes to all [Session](../../mozilla.components.browser.session/-session/index.md) instances and watches changes to their [Media](../../mozilla.components.concept.engine.media/-media/index.md) to create an
aggregated [MediaState](../-media-state/index.md).

Other components can subscribe to the state machine to get notified about [MediaState](../-media-state/index.md) changes.

