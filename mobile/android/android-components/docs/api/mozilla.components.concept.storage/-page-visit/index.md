[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [PageVisit](./index.md)

# PageVisit

`data class PageVisit` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L138)

Information to record about a visit.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PageVisit(visitType: `[`VisitType`](../-visit-type/index.md)`, redirectSource: `[`RedirectSource`](../-redirect-source/index.md)`)`<br>Information to record about a visit. |

### Properties

| Name | Summary |
|---|---|
| [redirectSource](redirect-source.md) | `val redirectSource: `[`RedirectSource`](../-redirect-source/index.md)<br>If this visit is redirecting to another page, what kind of redirect is it? See [RedirectSource](../-redirect-source/index.md) for the options. |
| [visitType](visit-type.md) | `val visitType: `[`VisitType`](../-visit-type/index.md)<br>The transition type for this visit. See [VisitType](../-visit-type/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
