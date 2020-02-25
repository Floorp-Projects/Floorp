[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissions](./index.md)

# SitePermissions

`data class SitePermissions` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissions.kt#L16)

A site permissions and its state.

### Types

| Name | Summary |
|---|---|
| [Status](-status/index.md) | `enum class Status` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SitePermissions(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, location: `[`Status`](-status/index.md)` = NO_DECISION, notification: `[`Status`](-status/index.md)` = NO_DECISION, microphone: `[`Status`](-status/index.md)` = NO_DECISION, camera: `[`Status`](-status/index.md)` = NO_DECISION, bluetooth: `[`Status`](-status/index.md)` = NO_DECISION, localStorage: `[`Status`](-status/index.md)` = NO_DECISION, autoplayAudible: `[`Status`](-status/index.md)` = NO_DECISION, autoplayInaudible: `[`Status`](-status/index.md)` = NO_DECISION, savedAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)`<br>A site permissions and its state. |

### Properties

| Name | Summary |
|---|---|
| [autoplayAudible](autoplay-audible.md) | `val autoplayAudible: `[`Status`](-status/index.md) |
| [autoplayInaudible](autoplay-inaudible.md) | `val autoplayInaudible: `[`Status`](-status/index.md) |
| [bluetooth](bluetooth.md) | `val bluetooth: `[`Status`](-status/index.md) |
| [camera](camera.md) | `val camera: `[`Status`](-status/index.md) |
| [localStorage](local-storage.md) | `val localStorage: `[`Status`](-status/index.md) |
| [location](location.md) | `val location: `[`Status`](-status/index.md) |
| [microphone](microphone.md) | `val microphone: `[`Status`](-status/index.md) |
| [notification](notification.md) | `val notification: `[`Status`](-status/index.md) |
| [origin](origin.md) | `val origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [savedAt](saved-at.md) | `val savedAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Functions

| Name | Summary |
|---|---|
| [get](get.md) | `operator fun get(permissionType: `[`Permission`](../-site-permissions-storage/-permission/index.md)`): `[`Status`](-status/index.md)<br>Gets the current status for a [Permission](../-site-permissions-storage/-permission/index.md) type |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
