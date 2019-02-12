[android-components](../../index.md) / [mozilla.components.concept.toolbar](../index.md) / [AutocompleteDelegate](./index.md)

# AutocompleteDelegate

`interface AutocompleteDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/AutocompleteDelegate.kt#L11)

Describes an object to which a [AutocompleteResult](../-autocomplete-result/index.md) may be applied.
Usually, this will delegate to a specific text view.

### Functions

| Name | Summary |
|---|---|
| [applyAutocompleteResult](apply-autocomplete-result.md) | `abstract fun applyAutocompleteResult(result: `[`AutocompleteResult`](../-autocomplete-result/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [noAutocompleteResult](no-autocomplete-result.md) | `abstract fun noAutocompleteResult(input: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Autocompletion was invoked and no match was returned. |

### Inheritors

| Name | Summary |
|---|---|
| [AsyncAutocompleteDelegate](../../mozilla.components.browser.toolbar/-async-autocomplete-delegate/index.md) | `class AsyncAutocompleteDelegate : `[`AutocompleteDelegate`](./index.md)`, CoroutineScope`<br>An autocomplete delegate which is aware of its parent scope (to check for cancellations). Responsible for processing autocompletion results and discarding stale results when [urlView](#) moved on. |
