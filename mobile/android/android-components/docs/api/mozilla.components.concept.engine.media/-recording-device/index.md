[android-components](../../index.md) / [mozilla.components.concept.engine.media](../index.md) / [RecordingDevice](./index.md)

# RecordingDevice

`data class RecordingDevice` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/media/RecordingDevice.kt#L13)

A recording device that can be used by web content.

### Types

| Name | Summary |
|---|---|
| [Status](-status/index.md) | `enum class Status`<br>States a recording device can be in. |
| [Type](-type/index.md) | `enum class Type`<br>Types of recording devices. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RecordingDevice(type: `[`Type`](-type/index.md)`, status: `[`Status`](-status/index.md)`)`<br>A recording device that can be used by web content. |

### Properties

| Name | Summary |
|---|---|
| [status](status.md) | `val status: `[`Status`](-status/index.md)<br>The status of the recording device (e.g. whether this device is recording) |
| [type](type.md) | `val type: `[`Type`](-type/index.md)<br>The type of recording device (e.g. camera or microphone) |
