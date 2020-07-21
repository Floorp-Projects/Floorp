[android-components](../../index.md) / [mozilla.components.concept.engine.content.blocking](../index.md) / [TrackingProtectionException](./index.md)

# TrackingProtectionException

`interface TrackingProtectionException` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/content/blocking/TrackingProtectionException.kt#L10)

Represents a site that will be ignored by the tracking protection policies.

### Properties

| Name | Summary |
|---|---|
| [url](url.md) | `abstract val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The url of the site to be ignored. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [GeckoTrackingProtectionException](../../mozilla.components.browser.engine.gecko.content.blocking/-gecko-tracking-protection-exception/index.md) | `data class GeckoTrackingProtectionException : `[`TrackingProtectionException`](./index.md)<br>Represents a site that will be ignored by the tracking protection policies. |
