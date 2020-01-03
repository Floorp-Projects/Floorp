[android-components](../../index.md) / [mozilla.components.feature.media](../index.md) / [RecordingDevicesNotificationFeature](./index.md)

# RecordingDevicesNotificationFeature

`class RecordingDevicesNotificationFeature` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/media/src/main/java/mozilla/components/feature/media/RecordingDevicesNotificationFeature.kt#L28)

Feature for displaying an ongoing notification while recording devices (camera, microphone) are used.

This feature should be initialized globally (application scope) and only once.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `RecordingDevicesNotificationFeature(context: <ERROR CLASS>, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`)`<br>Feature for displaying an ongoing notification while recording devices (camera, microphone) are used. |

### Functions

| Name | Summary |
|---|---|
| [enable](enable.md) | `fun enable(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enable the media notification feature. An ongoing notification will be shown while recording devices (camera, microphone) are used. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
