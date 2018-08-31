---
title: Session.FindResult - 
---

[mozilla.components.browser.session](../../index.html) / [Session](../index.html) / [FindResult](./index.html)

# FindResult

`data class FindResult`

A value type representing a result of a "find in page" operation.

### Constructors

| [&lt;init&gt;](-init-.html) | `FindResult(activeMatchOrdinal: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, numberOfMatches: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, isDoneCounting: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>A value type representing a result of a "find in page" operation. |

### Properties

| [activeMatchOrdinal](active-match-ordinal.html) | `val activeMatchOrdinal: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the zero-based ordinal of the currently selected match. |
| [isDoneCounting](is-done-counting.html) | `val isDoneCounting: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>true if the find operation has completed, otherwise false. |
| [numberOfMatches](number-of-matches.html) | `val numberOfMatches: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the match count |

