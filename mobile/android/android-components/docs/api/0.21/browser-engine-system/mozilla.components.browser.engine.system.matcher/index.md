---
title: mozilla.components.browser.engine.system.matcher - 
---

[mozilla.components.browser.engine.system.matcher](./index.html)

## Package mozilla.components.browser.engine.system.matcher

### Types

| [ReversibleString](-reversible-string/index.html) | `abstract class ReversibleString`<br>A String wrapper utility that allows for efficient string reversal. We regularly need to reverse strings. The standard way of doing this in Java would be to copy the string to reverse (e.g. using StringBuffer.reverse()). This seems wasteful when we only read our Strings character by character, in which case can just transpose positions as needed. |
| [Trie](-trie/index.html) | `open class Trie`<br>Simple implementation of a Trie, used for indexing URLs. |
| [UrlMatcher](-url-matcher/index.html) | `class UrlMatcher : OnSharedPreferenceChangeListener`<br>Provides functionality to process categorized URL black/white lists and match URLs against these lists. |

### Extensions for External Classes

| [kotlin.String](kotlin.-string/index.html) |  |

