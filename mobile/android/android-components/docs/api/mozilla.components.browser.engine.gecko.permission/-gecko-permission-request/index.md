[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.permission](../index.md) / [GeckoPermissionRequest](./index.md)

# GeckoPermissionRequest

`sealed class GeckoPermissionRequest : `[`PermissionRequest`](../../mozilla.components.concept.engine.permission/-permission-request/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/permission/GeckoPermissionRequest.kt#L31)

Gecko-based implementation of [PermissionRequest](../../mozilla.components.concept.engine.permission/-permission-request/index.md).

### Types

| Name | Summary |
|---|---|
| [App](-app/index.md) | `data class App : `[`GeckoPermissionRequest`](./index.md)<br>Represents a gecko-based application permission request. |
| [Content](-content/index.md) | `data class Content : `[`GeckoPermissionRequest`](./index.md)<br>Represents a gecko-based content permission request. |
| [Media](-media/index.md) | `data class Media : `[`GeckoPermissionRequest`](./index.md)<br>Represents a gecko-based media permission request. |

### Properties

| Name | Summary |
|---|---|
| [permissions](permissions.md) | `open val permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../mozilla.components.concept.engine.permission/-permission/index.md)`>`<br>the list of requested permissions. |

### Inherited Properties

| Name | Summary |
|---|---|
| [uri](../../mozilla.components.concept.engine.permission/-permission-request/uri.md) | `abstract val uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The origin URI which caused the permissions to be requested. |

### Functions

| Name | Summary |
|---|---|
| [grant](grant.md) | `open fun grant(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../mozilla.components.concept.engine.permission/-permission/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Grants the provided permissions, or all requested permissions, if none are provided. |
| [reject](reject.md) | `open fun reject(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Rejects the requested permissions. |

### Inherited Functions

| Name | Summary |
|---|---|
| [containsVideoAndAudioSources](../../mozilla.components.concept.engine.permission/-permission-request/contains-video-and-audio-sources.md) | `open fun containsVideoAndAudioSources(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [grantIf](../../mozilla.components.concept.engine.permission/-permission-request/grant-if.md) | `open fun grantIf(predicate: (`[`Permission`](../../mozilla.components.concept.engine.permission/-permission/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Grants this permission request if the provided predicate is true for any of the requested permissions. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [App](-app/index.md) | `data class App : `[`GeckoPermissionRequest`](./index.md)<br>Represents a gecko-based application permission request. |
| [Content](-content/index.md) | `data class Content : `[`GeckoPermissionRequest`](./index.md)<br>Represents a gecko-based content permission request. |
| [Media](-media/index.md) | `data class Media : `[`GeckoPermissionRequest`](./index.md)<br>Represents a gecko-based media permission request. |
