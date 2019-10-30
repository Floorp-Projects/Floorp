[android-components](../../index.md) / [mozilla.components.browser.state.state.content](../index.md) / [FindResultState](./index.md)

# FindResultState

`data class FindResultState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/content/FindResultState.kt#L14)

A value type representing a result of a "find in page" operation.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FindResultState(activeMatchOrdinal: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, numberOfMatches: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, isDoneCounting: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`)`<br>A value type representing a result of a "find in page" operation. |

### Properties

| Name | Summary |
|---|---|
| [activeMatchOrdinal](active-match-ordinal.md) | `val activeMatchOrdinal: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the zero-based ordinal of the currently selected match. |
| [isDoneCounting](is-done-counting.md) | `val isDoneCounting: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>true if the find operation has completed, otherwise false. |
| [numberOfMatches](number-of-matches.md) | `val numberOfMatches: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>the match count |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
