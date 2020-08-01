[android-components](../../../index.md) / [mozilla.components.concept.engine.manifest](../../index.md) / [WebAppManifest](../index.md) / [ExternalApplicationResource](./index.md)

# ExternalApplicationResource

`data class ExternalApplicationResource` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifest.kt#L178)

An external native application that is related to the web app.

### Types

| Name | Summary |
|---|---|
| [Fingerprint](-fingerprint/index.md) | `data class Fingerprint`<br>Represents a set of cryptographic fingerprints used for verifying the application. The syntax and semantics of [type](-fingerprint/type.md) and [value](-fingerprint/value.md) are platform-defined. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ExternalApplicationResource(platform: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, minVersion: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, fingerprints: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Fingerprint`](-fingerprint/index.md)`> = emptyList())`<br>An external native application that is related to the web app. |

### Properties

| Name | Summary |
|---|---|
| [fingerprints](fingerprints.md) | `val fingerprints: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Fingerprint`](-fingerprint/index.md)`>`<br>[Fingerprint](-fingerprint/index.md) objects used for verifying the application. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Information additional to or instead of the URL, depending on the platform. |
| [minVersion](min-version.md) | `val minVersion: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The minimum version of an application related to this web app. |
| [platform](platform.md) | `val platform: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The platform the native app is associated with. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The URL where it can be found. |
