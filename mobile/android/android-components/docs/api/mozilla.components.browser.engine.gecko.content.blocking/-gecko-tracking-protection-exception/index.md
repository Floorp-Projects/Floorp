[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.content.blocking](../index.md) / [GeckoTrackingProtectionException](./index.md)

# GeckoTrackingProtectionException

`data class GeckoTrackingProtectionException : `[`TrackingProtectionException`](../../mozilla.components.concept.engine.content.blocking/-tracking-protection-exception/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/content/blocking/GeckoTrackingProtectionException.kt#L14)

Represents a site that will be ignored by the tracking protection policies.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GeckoTrackingProtectionException(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, principal: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "")`<br>Represents a site that will be ignored by the tracking protection policies. |

### Properties

| Name | Summary |
|---|---|
| [principal](principal.md) | `val principal: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Internal gecko identifier of an URI. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The url of the site to be ignored. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
