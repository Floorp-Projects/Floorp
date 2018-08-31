---
title: Trie - 
---

[mozilla.components.browser.engine.system.matcher](../index.html) / [Trie](./index.html)

# Trie

`open class Trie`

Simple implementation of a Trie, used for indexing URLs.

### Constructors

| [&lt;init&gt;](-init-.html) | `Trie(character: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`, parent: `[`Trie`](./index.md)`?)`<br>Simple implementation of a Trie, used for indexing URLs. |

### Properties

| [children](children.html) | `val children: SparseArray<`[`Trie`](./index.md)`>` |

### Functions

| [createNode](create-node.html) | `open fun createNode(character: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`, parent: `[`Trie`](./index.md)`): `[`Trie`](./index.md)<br>Creates a new node for the provided character and parent node. |
| [findNode](find-node.html) | `fun findNode(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Trie`](./index.md)`?`<br>`fun findNode(string: `[`ReversibleString`](../-reversible-string/index.html)`): `[`Trie`](./index.md)`?`<br>Finds the node corresponding to the provided string. |
| [put](put.html) | `fun put(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Trie`](./index.md)<br>`fun put(string: `[`ReversibleString`](../-reversible-string/index.html)`): `[`Trie`](./index.md)<br>Adds new nodes (recursively) for all chars in the provided string.`fun put(character: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`): `[`Trie`](./index.md)<br>Adds a new node for the provided character if none exists. |

### Companion Object Functions

| [createRootNode](create-root-node.html) | `fun createRootNode(): `[`Trie`](./index.md)<br>Creates a new root node. |

