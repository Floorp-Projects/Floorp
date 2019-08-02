[android-components](../index.md) / [mozilla.components.feature.media.state](./index.md)

## Package mozilla.components.feature.media.state

### Types

| Name | Summary |
|---|---|
| [MediaState](-media-state/index.md) | `sealed class MediaState`<br>Accumulated state of all [Media](../mozilla.components.concept.engine.media/-media/index.md) of all [Session](../mozilla.components.browser.session/-session/index.md)s. |
| [MediaStateMachine](-media-state-machine/index.md) | `object MediaStateMachine : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-media-state-machine/-observer/index.md)`>`<br>A state machine that subscribes to all [Session](../mozilla.components.browser.session/-session/index.md) instances and watches changes to their [Media](../mozilla.components.concept.engine.media/-media/index.md) to create an aggregated [MediaState](-media-state/index.md). |
