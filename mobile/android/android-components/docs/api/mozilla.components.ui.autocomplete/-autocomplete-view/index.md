[android-components](../../index.md) / [mozilla.components.ui.autocomplete](../index.md) / [AutocompleteView](./index.md)

# AutocompleteView

`interface AutocompleteView` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/autocomplete/src/main/java/mozilla/components/ui/autocomplete/InlineAutocompleteEditText.kt#L48)

Aids in testing functionality which relies on some aspects of InlineAutocompleteEditText.

### Properties

| Name | Summary |
|---|---|
| [originalText](original-text.md) | `abstract val originalText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Current text. |

### Functions

| Name | Summary |
|---|---|
| [applyAutocompleteResult](apply-autocomplete-result.md) | `abstract fun applyAutocompleteResult(result: `[`AutocompleteResult`](../-inline-autocomplete-edit-text/-autocomplete-result/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Apply provided [result](apply-autocomplete-result.md#mozilla.components.ui.autocomplete.AutocompleteView$applyAutocompleteResult(mozilla.components.ui.autocomplete.InlineAutocompleteEditText.AutocompleteResult)/result) autocomplete result. |
| [noAutocompleteResult](no-autocomplete-result.md) | `abstract fun noAutocompleteResult(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notify that there is no autocomplete result available. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [InlineAutocompleteEditText](../-inline-autocomplete-edit-text/index.md) | `open class InlineAutocompleteEditText : AppCompatEditText, `[`AutocompleteView`](./index.md)<br>A UI edit text component which supports inline autocompletion. |
