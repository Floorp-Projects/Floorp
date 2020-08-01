[android-components](../../index.md) / [mozilla.components.browser.icons.preparer](../index.md) / [MemoryIconPreparer](./index.md)

# MemoryIconPreparer

`class MemoryIconPreparer : `[`IconPreprarer`](../-icon-preprarer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/preparer/MemoryIconPreparer.kt#L14)

An [IconPreprarer](../-icon-preprarer/index.md) implementation that will add known resource URLs (from an in-memory cache) to the request if the
request doesn't contain a list of resources yet.

### Types

| Name | Summary |
|---|---|
| [PreparerMemoryCache](-preparer-memory-cache/index.md) | `interface PreparerMemoryCache` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `MemoryIconPreparer(cache: `[`PreparerMemoryCache`](-preparer-memory-cache/index.md)`)`<br>An [IconPreprarer](../-icon-preprarer/index.md) implementation that will add known resource URLs (from an in-memory cache) to the request if the request doesn't contain a list of resources yet. |

### Functions

| Name | Summary |
|---|---|
| [prepare](prepare.md) | `fun prepare(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
