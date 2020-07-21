[android-components](../../index.md) / [mozilla.components.browser.domains](../index.md) / [DomainAutoCompleteProvider](index.md) / [autocomplete](./autocomplete.md)

# autocomplete

`fun autocomplete(rawText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Result`](-result/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/domains/src/main/java/mozilla/components/browser/domains/DomainAutoCompleteProvider.kt#L61)

Computes an autocomplete suggestion for the given text, and invokes the
provided callback, passing the result.

### Parameters

`rawText` - text to be auto completed

**Return**
the result of auto-completion. If no match is found an empty
result object is returned.

