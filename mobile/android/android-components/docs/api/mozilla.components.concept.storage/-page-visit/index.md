[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [PageVisit](./index.md)

# PageVisit

`data class PageVisit` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/HistoryStorage.kt#L129)

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
