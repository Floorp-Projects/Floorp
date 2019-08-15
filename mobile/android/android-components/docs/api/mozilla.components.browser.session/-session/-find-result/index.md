[android-components](../../../index.md) / [mozilla.components.browser.session](../../index.md) / [Session](../index.md) / [FindResult](./index.md)

# FindResult

`data class FindResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/Session.kt#L175)

A value type representing a result of a "find in page" operation.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FindResult(activeMatchOrdinal: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, numberOfMatches: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, isDoneCounting: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>A value type representing a result of a "find in page" operation. |

### Properties

| Name | Summary |
|---|---|
| [activeMatchOrdinal](active-match-ordinal.md) | `val activeMatchOrdinal: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the zero-based ordinal of the currently selected match. |
| [isDoneCounting](is-done-counting.md) | `val isDoneCounting: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>true if the find operation has completed, otherwise false. |
| [numberOfMatches](number-of-matches.md) | `val numberOfMatches: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the match count |
