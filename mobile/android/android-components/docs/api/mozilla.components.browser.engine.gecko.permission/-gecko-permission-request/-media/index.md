[android-components](../../../index.md) / [mozilla.components.browser.engine.gecko.permission](../../index.md) / [GeckoPermissionRequest](../index.md) / [Media](./index.md)

# Media

`data class Media : `[`GeckoPermissionRequest`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/permission/GeckoPermissionRequest.kt#L99)

Represents a gecko-based media permission request.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Media(uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, videoSources: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`MediaSource`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession/PermissionDelegate/MediaSource.html)`>, audioSources: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`MediaSource`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession/PermissionDelegate/MediaSource.html)`>, callback: `[`MediaCallback`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession/PermissionDelegate/MediaCallback.html)`)`<br>Represents a gecko-based media permission request. |

### Properties

| Name | Summary |
|---|---|
| [uri](uri.md) | `val uri: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the URI of the content requesting the permissions. |

### Inherited Properties

| Name | Summary |
|---|---|
| [permissions](../permissions.md) | `open val permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../../mozilla.components.concept.engine.permission/-permission/index.md)`>`<br>the list of requested permissions. |

### Functions

| Name | Summary |
|---|---|
| [containsVideoAndAudioSources](contains-video-and-audio-sources.md) | `fun containsVideoAndAudioSources(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [grant](grant.md) | `fun grant(permissions: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Permission`](../../../mozilla.components.concept.engine.permission/-permission/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Grants the provided permissions, or all requested permissions, if none are provided. |
| [reject](reject.md) | `fun reject(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Rejects the requested permissions. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [mapPermission](map-permission.md) | `fun mapPermission(mediaSource: `[`MediaSource`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession/PermissionDelegate/MediaSource.html)`): `[`Permission`](../../../mozilla.components.concept.engine.permission/-permission/index.md) |
