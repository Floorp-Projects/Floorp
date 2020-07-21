[android-components](../../index.md) / [mozilla.components.service.digitalassetlinks.api](../index.md) / [ListStatementsResponse](./index.md)

# ListStatementsResponse

`data class ListStatementsResponse` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/digitalassetlinks/src/main/java/mozilla/components/service/digitalassetlinks/api/ListStatementsResponse.kt#L19)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ListStatementsResponse(statements: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Statement`](../../mozilla.components.service.digitalassetlinks/-statement/index.md)`>, maxAge: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, debug: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)` |

### Properties

| Name | Summary |
|---|---|
| [debug](debug.md) | `val debug: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Human-readable message containing information about the response. |
| [maxAge](max-age.md) | `val maxAge: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>From serving time, how much longer the response should be considered valid barring further updates. Formatted as a duration in seconds with up to nine fractional digits, terminated by 's'. Example: "3.5s". |
| [statements](statements.md) | `val statements: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Statement`](../../mozilla.components.service.digitalassetlinks/-statement/index.md)`>`<br>A list of all the matching statements that have been found. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
