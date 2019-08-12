[android-components](../../../index.md) / [mozilla.components.concept.engine.media](../../index.md) / [Media](../index.md) / [Controller](./index.md)

# Controller

`interface Controller` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/media/Media.kt#L45)

Controller for controlling playback of a media element.

### Functions

| Name | Summary |
|---|---|
| [pause](pause.md) | `abstract fun pause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Pauses the media. |
| [play](play.md) | `abstract fun play(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Plays the media. |
| [seek](seek.md) | `abstract fun seek(time: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Seek the media to a given time. |
| [setMuted](set-muted.md) | `abstract fun setMuted(muted: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Mutes/Unmutes the media. |
