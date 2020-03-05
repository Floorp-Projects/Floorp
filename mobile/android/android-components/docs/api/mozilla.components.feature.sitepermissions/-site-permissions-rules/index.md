[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsRules](./index.md)

# SitePermissionsRules

`data class SitePermissionsRules` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsRules.kt#L15)

Indicate how site permissions must behave by permission category.

### Types

| Name | Summary |
|---|---|
| [Action](-action/index.md) | `enum class Action` |
| [AutoplayAction](-autoplay-action/index.md) | `enum class AutoplayAction`<br>Autoplay requests will never prompt the user |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SitePermissionsRules(camera: `[`Action`](-action/index.md)`, location: `[`Action`](-action/index.md)`, notification: `[`Action`](-action/index.md)`, microphone: `[`Action`](-action/index.md)`, autoplayAudible: `[`AutoplayAction`](-autoplay-action/index.md)`, autoplayInaudible: `[`AutoplayAction`](-autoplay-action/index.md)`)` |

### Properties

| Name | Summary |
|---|---|
| [autoplayAudible](autoplay-audible.md) | `val autoplayAudible: `[`Action`](-action/index.md) |
| [autoplayInaudible](autoplay-inaudible.md) | `val autoplayInaudible: `[`Action`](-action/index.md) |
| [camera](camera.md) | `val camera: `[`Action`](-action/index.md) |
| [location](location.md) | `val location: `[`Action`](-action/index.md) |
| [microphone](microphone.md) | `val microphone: `[`Action`](-action/index.md) |
| [notification](notification.md) | `val notification: `[`Action`](-action/index.md) |

### Functions

| Name | Summary |
|---|---|
| [toSitePermissions](to-site-permissions.md) | `fun toSitePermissions(origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, savedAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis()): `[`SitePermissions`](../-site-permissions/index.md)<br>Converts a [SitePermissionsRules](./index.md) object into a [SitePermissions](../-site-permissions/index.md) . |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
