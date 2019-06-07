[android-components](../../index.md) / [mozilla.components.feature.media.state](../index.md) / [MediaState](./index.md)

# MediaState

`sealed class MediaState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/state/MediaState.kt#L13)

Accumulated state of all [Media](../../mozilla.components.concept.engine.media/-media/index.md) of all [Session](../../mozilla.components.browser.session/-session/index.md)s.

### Types

| Name | Summary |
|---|---|
| [None](-none/index.md) | `object None : `[`MediaState`](./index.md)<br>None: No media state. |
| [Paused](-paused/index.md) | `data class Paused : `[`MediaState`](./index.md)<br>Paused: [media](-paused/media.md) of [session](-paused/session.md) is currently paused. |
| [Playing](-playing/index.md) | `data class Playing : `[`MediaState`](./index.md)<br>Playing: [media](-playing/media.md) of [session](-playing/session.md) is currently playing. |

### Inheritors

| Name | Summary |
|---|---|
| [None](-none/index.md) | `object None : `[`MediaState`](./index.md)<br>None: No media state. |
| [Paused](-paused/index.md) | `data class Paused : `[`MediaState`](./index.md)<br>Paused: [media](-paused/media.md) of [session](-paused/session.md) is currently paused. |
| [Playing](-playing/index.md) | `data class Playing : `[`MediaState`](./index.md)<br>Playing: [media](-playing/media.md) of [session](-playing/session.md) is currently playing. |
