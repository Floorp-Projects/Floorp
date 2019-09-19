[android-components](../../index.md) / [mozilla.components.concept.engine.permission](../index.md) / [Permission](./index.md)

# Permission

`sealed class Permission` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/permission/PermissionRequest.kt#L60)

Represents all the different supported permission types.

### Types

| Name | Summary |
|---|---|
| [AppAudio](-app-audio/index.md) | `data class AppAudio : `[`Permission`](./index.md) |
| [AppCamera](-app-camera/index.md) | `data class AppCamera : `[`Permission`](./index.md) |
| [AppLocationCoarse](-app-location-coarse/index.md) | `data class AppLocationCoarse : `[`Permission`](./index.md) |
| [AppLocationFine](-app-location-fine/index.md) | `data class AppLocationFine : `[`Permission`](./index.md) |
| [ContentAudioCapture](-content-audio-capture/index.md) | `data class ContentAudioCapture : `[`Permission`](./index.md) |
| [ContentAudioMicrophone](-content-audio-microphone/index.md) | `data class ContentAudioMicrophone : `[`Permission`](./index.md) |
| [ContentAudioOther](-content-audio-other/index.md) | `data class ContentAudioOther : `[`Permission`](./index.md) |
| [ContentGeoLocation](-content-geo-location/index.md) | `data class ContentGeoLocation : `[`Permission`](./index.md) |
| [ContentNotification](-content-notification/index.md) | `data class ContentNotification : `[`Permission`](./index.md) |
| [ContentProtectedMediaId](-content-protected-media-id/index.md) | `data class ContentProtectedMediaId : `[`Permission`](./index.md) |
| [ContentVideoApplication](-content-video-application/index.md) | `data class ContentVideoApplication : `[`Permission`](./index.md) |
| [ContentVideoBrowser](-content-video-browser/index.md) | `data class ContentVideoBrowser : `[`Permission`](./index.md) |
| [ContentVideoCamera](-content-video-camera/index.md) | `data class ContentVideoCamera : `[`Permission`](./index.md) |
| [ContentVideoCapture](-content-video-capture/index.md) | `data class ContentVideoCapture : `[`Permission`](./index.md) |
| [ContentVideoOther](-content-video-other/index.md) | `data class ContentVideoOther : `[`Permission`](./index.md) |
| [ContentVideoScreen](-content-video-screen/index.md) | `data class ContentVideoScreen : `[`Permission`](./index.md) |
| [ContentVideoWindow](-content-video-window/index.md) | `data class ContentVideoWindow : `[`Permission`](./index.md) |
| [Generic](-generic/index.md) | `data class Generic : `[`Permission`](./index.md) |

### Properties

| Name | Summary |
|---|---|
| [desc](desc.md) | `abstract val desc: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>an optional description of what this permission type is for. |
| [id](id.md) | `abstract val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>an optional native engine-specific ID of this permission. |

### Inheritors

| Name | Summary |
|---|---|
| [AppAudio](-app-audio/index.md) | `data class AppAudio : `[`Permission`](./index.md) |
| [AppCamera](-app-camera/index.md) | `data class AppCamera : `[`Permission`](./index.md) |
| [AppLocationCoarse](-app-location-coarse/index.md) | `data class AppLocationCoarse : `[`Permission`](./index.md) |
| [AppLocationFine](-app-location-fine/index.md) | `data class AppLocationFine : `[`Permission`](./index.md) |
| [ContentAudioCapture](-content-audio-capture/index.md) | `data class ContentAudioCapture : `[`Permission`](./index.md) |
| [ContentAudioMicrophone](-content-audio-microphone/index.md) | `data class ContentAudioMicrophone : `[`Permission`](./index.md) |
| [ContentAudioOther](-content-audio-other/index.md) | `data class ContentAudioOther : `[`Permission`](./index.md) |
| [ContentGeoLocation](-content-geo-location/index.md) | `data class ContentGeoLocation : `[`Permission`](./index.md) |
| [ContentNotification](-content-notification/index.md) | `data class ContentNotification : `[`Permission`](./index.md) |
| [ContentProtectedMediaId](-content-protected-media-id/index.md) | `data class ContentProtectedMediaId : `[`Permission`](./index.md) |
| [ContentVideoApplication](-content-video-application/index.md) | `data class ContentVideoApplication : `[`Permission`](./index.md) |
| [ContentVideoBrowser](-content-video-browser/index.md) | `data class ContentVideoBrowser : `[`Permission`](./index.md) |
| [ContentVideoCamera](-content-video-camera/index.md) | `data class ContentVideoCamera : `[`Permission`](./index.md) |
| [ContentVideoCapture](-content-video-capture/index.md) | `data class ContentVideoCapture : `[`Permission`](./index.md) |
| [ContentVideoOther](-content-video-other/index.md) | `data class ContentVideoOther : `[`Permission`](./index.md) |
| [ContentVideoScreen](-content-video-screen/index.md) | `data class ContentVideoScreen : `[`Permission`](./index.md) |
| [ContentVideoWindow](-content-video-window/index.md) | `data class ContentVideoWindow : `[`Permission`](./index.md) |
| [Generic](-generic/index.md) | `data class Generic : `[`Permission`](./index.md) |
