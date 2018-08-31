---
title: DomainAutoCompleteProvider.Result - 
---

[mozilla.components.browser.domains](../../index.html) / [DomainAutoCompleteProvider](../index.html) / [Result](./index.html)

# Result

`data class Result`

Represents a result of auto-completion.

### Constructors

| [&lt;init&gt;](-init-.html) | `Result(text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, source: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Represents a result of auto-completion. |

### Properties

| [size](size.html) | `val size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>total number of available autocomplete domains in this source |
| [source](source.html) | `val source: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the source identifier of the autocomplete source |
| [text](text.html) | `val text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the result text starting with the raw search text as passed to [autocomplete](../autocomplete.html) followed by the completion text (e.g. moz =&gt; mozilla.org) |
| [url](url.html) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the complete url (containing the protocol) as provided when the domain was saved. (e.g. https://mozilla.org) |

