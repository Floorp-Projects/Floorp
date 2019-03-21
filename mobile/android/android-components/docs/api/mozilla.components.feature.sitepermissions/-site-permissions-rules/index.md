[android-components](../../index.md) / [mozilla.components.feature.sitepermissions](../index.md) / [SitePermissionsRules](./index.md)

# SitePermissionsRules

`data class SitePermissionsRules` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsRules.kt#L16)

Indicate how site permissions must behave by permission category.

### Types

| Name | Summary |
|---|---|
| [Action](-action/index.md) | `enum class Action` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SitePermissionsRules(camera: `[`Action`](-action/index.md)`, location: `[`Action`](-action/index.md)`, notification: `[`Action`](-action/index.md)`, microphone: `[`Action`](-action/index.md)`, exceptions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Uri`](https://developer.android.com/reference/android/net/Uri.html)`>? = null)`<br>Indicate how site permissions must behave by permission category. |

### Properties

| Name | Summary |
|---|---|
| [camera](camera.md) | `val camera: `[`Action`](-action/index.md) |
| [exceptions](exceptions.md) | `val exceptions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Uri`](https://developer.android.com/reference/android/net/Uri.html)`>?` |
| [location](location.md) | `val location: `[`Action`](-action/index.md) |
| [microphone](microphone.md) | `val microphone: `[`Action`](-action/index.md) |
| [notification](notification.md) | `val notification: `[`Action`](-action/index.md) |
