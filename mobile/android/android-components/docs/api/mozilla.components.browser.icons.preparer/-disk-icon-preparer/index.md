[android-components](../../index.md) / [mozilla.components.browser.icons.preparer](../index.md) / [DiskIconPreparer](./index.md)

# DiskIconPreparer

`class DiskIconPreparer : `[`IconPreprarer`](../-icon-preprarer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/preparer/DiskIconPreparer.kt#L14)

[IconPreprarer](../-icon-preprarer/index.md) implementation implementation that will add known resource URLs (from a disk cache) to the request
if the request doesn't contain a list of resources yet.

### Types

| Name | Summary |
|---|---|
| [PreparerDiskCache](-preparer-disk-cache/index.md) | `interface PreparerDiskCache` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DiskIconPreparer(cache: `[`PreparerDiskCache`](-preparer-disk-cache/index.md)`)`<br>[IconPreprarer](../-icon-preprarer/index.md) implementation implementation that will add known resource URLs (from a disk cache) to the request if the request doesn't contain a list of resources yet. |

### Functions

| Name | Summary |
|---|---|
| [prepare](prepare.md) | `fun prepare(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
