[android-components](../../../index.md) / [mozilla.components.browser.domains](../../index.md) / [DomainAutoCompleteProvider](../index.md) / [Result](./index.md)

# Result

`data class Result` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/DomainAutoCompleteProvider.kt#L42)

Represents a result of auto-completion.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Result(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, source: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Represents a result of auto-completion. |

### Properties

| Name | Summary |
|---|---|
| [size](size.md) | `val size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>total number of available autocomplete domains in this source |
| [source](source.md) | `val source: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the source identifier of the autocomplete source |
| [text](text.md) | `val text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the result text starting with the raw search text as passed to [autocomplete](../autocomplete.md) followed by the completion text (e.g. moz =&gt; mozilla.org) |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the complete url (containing the protocol) as provided when the domain was saved. (e.g. https://mozilla.org) |
