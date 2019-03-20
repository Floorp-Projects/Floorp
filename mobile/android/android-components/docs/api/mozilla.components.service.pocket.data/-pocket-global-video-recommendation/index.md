[android-components](../../index.md) / [mozilla.components.service.pocket.data](../index.md) / [PocketGlobalVideoRecommendation](./index.md)

# PocketGlobalVideoRecommendation

`data class PocketGlobalVideoRecommendation` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/pocket/src/main/java/mozilla/components/service/pocket/data/PocketGlobalVideoRecommendation.kt#L26)

A recommended video as returned from the Pocket Global Video Recommendation endpoint v2.

### Types

| Name | Summary |
|---|---|
| [Author](-author/index.md) | `data class Author`<br>An author or publisher of a [PocketGlobalVideoRecommendation](./index.md). |

### Properties

| Name | Summary |
|---|---|
| [authors](authors.md) | `val authors: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Author`](-author/index.md)`>`<br>the authors or publishers of this recommendation; unclear if this can be empty. |
| [domain](domain.md) | `val domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the domain where the video appears, e.g. "youtube.com". |
| [excerpt](excerpt.md) | `val excerpt: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>a summary of the video. |
| [id](id.md) | `val id: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>a unique identifier for this recommendation. |
| [imageSrc](image-src.md) | `val imageSrc: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>a url to a still image representing the video. |
| [popularitySortId](popularity-sort-id.md) | `val popularitySortId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the index of this recommendation in the list if the list was sorted by popularity. |
| [publishedTimestamp](published-timestamp.md) | `val publishedTimestamp: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>unknown: please ask for clarification if needed. This may be "0". |
| [sortId](sort-id.md) | `val sortId: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the index of this recommendation in the list which is sorted by date added to the API results. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the title of the video. |
| [tvURL](tv-u-r-l.md) | `val tvURL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>a url to the video on pages formatted for TV form factors (e.g. YouTube.com/tv). |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>a url to the video. |
