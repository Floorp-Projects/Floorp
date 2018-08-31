---
title: Trie.put - 
---

[mozilla.components.browser.engine.system.matcher](../index.html) / [Trie](index.html) / [put](./put.html)

# put

`fun put(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Trie`](index.html)
`fun put(string: `[`ReversibleString`](../-reversible-string/index.html)`): `[`Trie`](index.html)

Adds new nodes (recursively) for all chars in the provided string.

### Parameters

`string` - the string for which a node should be added.

**Return**
the newly created node or the existing one.

`fun put(character: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`): `[`Trie`](index.html)

Adds a new node for the provided character if none exists.

### Parameters

`character` - the character for which a node should be added.

**Return**
the newly created node or the existing one.

