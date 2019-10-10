[android-components](../../../../index.md) / [mozilla.components.concept.engine.manifest](../../../index.md) / [WebAppManifest](../../index.md) / [ShareTarget](../index.md) / [Params](./index.md)

# Params

`data class Params` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifest.kt#L220)

Specifies what query parameters correspond to share data.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Params(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, files: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Files`](../-files/index.md)`> = emptyList())`<br>Specifies what query parameters correspond to share data. |

### Properties

| Name | Summary |
|---|---|
| [files](files.md) | `val files: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Files`](../-files/index.md)`>`<br>Form fields used to share files. |
| [text](text.md) | `val text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Name of the query parameter used for the body of the data being shared. |
| [title](title.md) | `val title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Name of the query parameter used for the title of the data being shared. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Name of the query parameter used for a URL referring to a shared resource. |
