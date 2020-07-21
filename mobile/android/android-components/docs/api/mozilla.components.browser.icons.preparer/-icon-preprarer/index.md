[android-components](../../index.md) / [mozilla.components.browser.icons.preparer](../index.md) / [IconPreprarer](./index.md)

# IconPreprarer

`interface IconPreprarer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/preparer/IconPreprarer.kt#L14)

An [IconPreparer](#) implementation receives an [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md) before it is getting loaded. The preparer has the option
to rewrite the [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md) and return a new instance.

### Functions

| Name | Summary |
|---|---|
| [prepare](prepare.md) | `abstract fun prepare(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`): `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DiskIconPreparer](../-disk-icon-preparer/index.md) | `class DiskIconPreparer : `[`IconPreprarer`](./index.md)<br>[IconPreprarer](./index.md) implementation implementation that will add known resource URLs (from a disk cache) to the request if the request doesn't contain a list of resources yet. |
| [MemoryIconPreparer](../-memory-icon-preparer/index.md) | `class MemoryIconPreparer : `[`IconPreprarer`](./index.md)<br>An [IconPreprarer](./index.md) implementation that will add known resource URLs (from an in-memory cache) to the request if the request doesn't contain a list of resources yet. |
| [TippyTopIconPreparer](../-tippy-top-icon-preparer/index.md) | `class TippyTopIconPreparer : `[`IconPreprarer`](./index.md)<br>[IconPreprarer](./index.md) implementation that looks up the host in our "tippy top" list. If it can find a match then it inserts the icon URL into the request. |
