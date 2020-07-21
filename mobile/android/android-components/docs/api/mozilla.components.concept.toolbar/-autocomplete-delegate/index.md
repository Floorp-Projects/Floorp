[android-components](../../index.md) / [mozilla.components.concept.toolbar](../index.md) / [AutocompleteDelegate](./index.md)

# AutocompleteDelegate

`interface AutocompleteDelegate` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/toolbar/src/main/java/mozilla/components/concept/toolbar/AutocompleteDelegate.kt#L11)

Describes an object to which a [AutocompleteResult](../-autocomplete-result/index.md) may be applied.
Usually, this will delegate to a specific text view.

### Functions

| Name | Summary |
|---|---|
| [applyAutocompleteResult](apply-autocomplete-result.md) | `abstract fun applyAutocompleteResult(result: `[`AutocompleteResult`](../-autocomplete-result/index.md)`, onApplied: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [noAutocompleteResult](no-autocomplete-result.md) | `abstract fun noAutocompleteResult(input: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Autocompletion was invoked and no match was returned. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
