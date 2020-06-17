[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks.api](../index.md) / [CheckAssetLinksResponse](./index.md)

# CheckAssetLinksResponse

`data class CheckAssetLinksResponse` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/api/CheckAssetLinksResponse.kt#L15)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CheckAssetLinksResponse(linked: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, maxAge: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, debug: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)` |

### Properties

| Name | Summary |
|---|---|
| [debug](debug.md) | `val debug: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Human-readable message containing information about the response. |
| [linked](linked.md) | `val linked: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>True if the assets specified in the request are linked by the relation specified in the request. |
| [maxAge](max-age.md) | `val maxAge: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>From serving time, how much longer the response should be considered valid barring further updates. Formatted as a duration in seconds with up to nine fractional digits, terminated by 's'. Example: "3.5s". |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
