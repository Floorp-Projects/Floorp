[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [VisitInfo](./index.md)

# VisitInfo

`data class VisitInfo` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L176)

Information about a history visit.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `VisitInfo(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, visitTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, visitType: `[`VisitType`](../-visit-type/index.md)`)`<br>Information about a history visit. |

### Properties

| Name | Summary |
|---|---|
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The title of the page that was visited, if known. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL of the page that was visited. |
| [visitTime](visit-time.md) | `val visitTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>The time the page was visited in integer milliseconds since the unix epoch. |
| [visitType](visit-type.md) | `val visitType: `[`VisitType`](../-visit-type/index.md)<br>What the transition type of the visit is, expressed as [VisitType](../-visit-type/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
