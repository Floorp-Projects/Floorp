[android-components](../../index.md) / [mozilla.components.lib.publicsuffixlist](../index.md) / [PublicSuffixList](./index.md)

# PublicSuffixList

`class PublicSuffixList` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/publicsuffixlist/src/main/java/mozilla/components/lib/publicsuffixlist/PublicSuffixList.kt#L26)

API for reading and accessing the public suffix list.

A "public suffix" is one under which Internet users can (or historically could) directly register names. Some
 examples of public suffixes are .com, .co.uk and pvt.k12.ma.us. The Public Suffix List is a list of all known
 public suffixes.

Note that this implementation applies the rules of the public suffix list only and does not validate domains.

https://publicsuffix.org/
https://github.com/publicsuffix/list

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `PublicSuffixList(context: <ERROR CLASS>, dispatcher: CoroutineDispatcher = Dispatchers.IO, scope: CoroutineScope = CoroutineScope(dispatcher))`<br>API for reading and accessing the public suffix list. |

### Functions

| Name | Summary |
|---|---|
| [getPublicSuffix](get-public-suffix.md) | `fun getPublicSuffix(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>`<br>Returns the public suffix of the given [domain](get-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$getPublicSuffix(kotlin.String)/domain); known as the effective top-level domain (eTLD). Returns `null` if the [domain](get-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$getPublicSuffix(kotlin.String)/domain) is a public suffix itself. |
| [getPublicSuffixPlusOne](get-public-suffix-plus-one.md) | `fun getPublicSuffixPlusOne(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?>`<br>Returns the public suffix and one more level; known as the registrable domain. Returns `null` if [domain](get-public-suffix-plus-one.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$getPublicSuffixPlusOne(kotlin.String)/domain) is a public suffix itself. |
| [isPublicSuffix](is-public-suffix.md) | `fun isPublicSuffix(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): Deferred<`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Returns true if the given [domain](is-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$isPublicSuffix(kotlin.String)/domain) is a public suffix; false otherwise. |
| [prefetch](prefetch.md) | `fun prefetch(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>`<br>Prefetch the public suffix list from disk so that it is available in memory. |
| [stripPublicSuffix](strip-public-suffix.md) | `fun stripPublicSuffix(domain: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): <ERROR CLASS>`<br>Strips the public suffix from the given [domain](strip-public-suffix.md#mozilla.components.lib.publicsuffixlist.PublicSuffixList$stripPublicSuffix(kotlin.String)/domain). Returns the original domain if no public suffix could be stripped. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
