---
title: DomainAutoCompleteProvider.autocomplete - 
---

[mozilla.components.browser.domains](../index.html) / [DomainAutoCompleteProvider](index.html) / [autocomplete](./autocomplete.html)

# autocomplete

`fun autocomplete(rawText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Result`](-result/index.html)

Computes an autocomplete suggestion for the given text, and invokes the
provided callback, passing the result.

### Parameters

`rawText` - text to be auto completed

**Return**
the result of auto-completion. If no match is found an empty
result object is returned.

