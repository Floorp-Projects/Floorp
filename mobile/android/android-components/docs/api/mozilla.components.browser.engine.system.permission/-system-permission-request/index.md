[android-components](../../index.md) / [mozilla.components.browser.engine.system.permission](../index.md) / [SystemPermissionRequest](./index.md)

# SystemPermissionRequest

`class SystemPermissionRequest : `[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/permission/SystemPermissionRequest.kt#L18)

WebView-based implementation of [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SystemPermissionRequest(nativeRequest: <ERROR CLASS>)`<br>WebView-based implementation of [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md). |

### Properties

| Name | Summary |
|---|---|
| [permissions](permissions.md) | `val permissions: <ERROR CLASS>`<br>List of requested permissions. |
| [uri](uri.md) | `val uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The origin URI which caused the permissions to be requested. |

### Functions

| Name | Summary |
|---|---|
| [grant](grant.md) | `fun grant(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../mozilla.components.concept.engine.permission/-permission/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Grants the provided permissions, or all requested permissions, if none are provided. |
| [reject](reject.md) | `fun reject(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Rejects the requested permissions. |

### Inherited Functions

| Name | Summary |
|---|---|
| [containsVideoAndAudioSources](../../mozilla.components.concept.engine.permission/-permission-request/contains-video-and-audio-sources.md) | `open fun containsVideoAndAudioSources(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [grantIf](../../mozilla.components.concept.engine.permission/-permission-request/grant-if.md) | `open fun grantIf(predicate: (`[`Permission`](../../mozilla.components.concept.engine.permission/-permission/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Grants this permission request if the provided predicate is true for any of the requested permissions. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [permissionsMap](permissions-map.md) | `val permissionsMap: <ERROR CLASS>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
