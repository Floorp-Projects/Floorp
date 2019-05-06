[android-components](../../index.md) / [mozilla.components.concept.engine.permission](../index.md) / [PermissionRequest](./index.md)

# PermissionRequest

`interface PermissionRequest` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/permission/PermissionRequest.kt#L11)

Represents a permission request, used when engines need access to protected
resources. Every request must be handled by either calling [grant](grant.md) or [reject](reject.md).

### Properties

| Name | Summary |
|---|---|
| [permissions](permissions.md) | `abstract val permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../-permission/index.md)`>`<br>List of requested permissions. |
| [uri](uri.md) | `abstract val uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The origin URI which caused the permissions to be requested. |

### Functions

| Name | Summary |
|---|---|
| [containsVideoAndAudioSources](contains-video-and-audio-sources.md) | `open fun containsVideoAndAudioSources(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [grant](grant.md) | `abstract fun grant(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../-permission/index.md)`> = this.permissions): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Grants the provided permissions, or all requested permissions, if none are provided. |
| [grantIf](grant-if.md) | `open fun grantIf(predicate: (`[`Permission`](../-permission/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Grants this permission request if the provided predicate is true for any of the requested permissions. |
| [reject](reject.md) | `abstract fun reject(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Rejects the requested permissions. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoPermissionRequest](../../mozilla.components.browser.engine.gecko.permission/-gecko-permission-request/index.md) | `sealed class GeckoPermissionRequest : `[`PermissionRequest`](./index.md)<br>Gecko-based implementation of [PermissionRequest](./index.md). |
| [SystemPermissionRequest](../../mozilla.components.browser.engine.system.permission/-system-permission-request/index.md) | `class SystemPermissionRequest : `[`PermissionRequest`](./index.md)<br>WebView-based implementation of [PermissionRequest](./index.md). |
