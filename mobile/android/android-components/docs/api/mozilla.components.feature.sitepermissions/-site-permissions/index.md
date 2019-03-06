[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissions](./index.md)

# SitePermissions

`data class SitePermissions` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissions.kt#L12)

A site permissions and its state.

### Types

| Name | Summary |
|---|---|
| [Status](-status/index.md) | `enum class Status` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SitePermissions(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, location: `[`Status`](-status/index.md)` = NO_DECISION, notification: `[`Status`](-status/index.md)` = NO_DECISION, microphone: `[`Status`](-status/index.md)` = NO_DECISION, cameraBack: `[`Status`](-status/index.md)` = NO_DECISION, cameraFront: `[`Status`](-status/index.md)` = NO_DECISION, bluetooth: `[`Status`](-status/index.md)` = NO_DECISION, localStorage: `[`Status`](-status/index.md)` = NO_DECISION, savedAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)`<br>A site permissions and its state. |

### Properties

| Name | Summary |
|---|---|
| [bluetooth](bluetooth.md) | `val bluetooth: `[`Status`](-status/index.md) |
| [cameraBack](camera-back.md) | `val cameraBack: `[`Status`](-status/index.md) |
| [cameraFront](camera-front.md) | `val cameraFront: `[`Status`](-status/index.md) |
| [localStorage](local-storage.md) | `val localStorage: `[`Status`](-status/index.md) |
| [location](location.md) | `val location: `[`Status`](-status/index.md) |
| [microphone](microphone.md) | `val microphone: `[`Status`](-status/index.md) |
| [notification](notification.md) | `val notification: `[`Status`](-status/index.md) |
| [origin](origin.md) | `val origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [savedAt](saved-at.md) | `val savedAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |
