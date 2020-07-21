[android-components](../../index.md) / [mozilla.components.browser.engine.system.matcher](../index.md) / [Trie](index.md) / [put](./put.md)

# put

`fun put(string: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Trie`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/Trie.kt#L57)
`fun put(string: `[`ReversibleString`](../-reversible-string/index.md)`): `[`Trie`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/Trie.kt#L67)

Adds new nodes (recursively) for all chars in the provided string.

### Parameters

`string` - the string for which a node should be added.

**Return**
the newly created node or the existing one.

`fun put(character: `[`Char`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char/index.html)`): `[`Trie`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/Trie.kt#L84)

Adds a new node for the provided character if none exists.

### Parameters

`character` - the character for which a node should be added.

**Return**
the newly created node or the existing one.

