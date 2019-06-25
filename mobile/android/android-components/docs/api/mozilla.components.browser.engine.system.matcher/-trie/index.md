[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [Trie](./index.md)

# Trie

`open class Trie` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/Trie.kt#L12)

Simple implementation of a Trie, used for indexing URLs.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Trie(character: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`, parent: `[`Trie`](./index.md)`?)`<br>Simple implementation of a Trie, used for indexing URLs. |

### Properties

| Name | Summary |
|---|---|
| [children](children.md) | `val children: <ERROR CLASS>` |

### Functions

| Name | Summary |
|---|---|
| [createNode](create-node.md) | `open fun createNode(character: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`, parent: `[`Trie`](./index.md)`): `[`Trie`](./index.md)<br>Creates a new node for the provided character and parent node. |
| [findNode](find-node.md) | `fun findNode(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Trie`](./index.md)`?`<br>`fun findNode(string: `[`ReversibleString`](../-reversible-string/index.md)`): `[`Trie`](./index.md)`?`<br>Finds the node corresponding to the provided string. |
| [put](put.md) | `fun put(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Trie`](./index.md)<br>`fun put(string: `[`ReversibleString`](../-reversible-string/index.md)`): `[`Trie`](./index.md)<br>Adds new nodes (recursively) for all chars in the provided string.`fun put(character: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`): `[`Trie`](./index.md)<br>Adds a new node for the provided character if none exists. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [createRootNode](create-root-node.md) | `fun createRootNode(): `[`Trie`](./index.md)<br>Creates a new root node. |
