[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [Trie](index.md) / [findNode](./find-node.md)

# findNode

`fun findNode(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Trie`](index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/Trie.kt#L26)
`fun findNode(string: `[`ReversibleString`](../-reversible-string/index.md)`): `[`Trie`](index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/Trie.kt#L36)

Finds the node corresponding to the provided string.

### Parameters

`string` - the string to search.

**Return**
the corresponding node if found, otherwise null.

