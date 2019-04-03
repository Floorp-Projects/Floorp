[android-components](../../index.md) / [mozilla.components.browser.icons.loader](../index.md) / [IconLoader](./index.md)

# IconLoader

`interface IconLoader` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/loader/IconLoader.kt#L13)

A loader that can load an icon from an [IconRequest.Resource](../../mozilla.components.browser.icons/-icon-request/-resource/index.md).

### Properties

| Name | Summary |
|---|---|
| [source](source.md) | `abstract val source: `[`Source`](../../mozilla.components.browser.icons/-icon/-source/index.md) |

### Functions

| Name | Summary |
|---|---|
| [load](load.md) | `abstract fun load(request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?`<br>Tries to load the [IconRequest.Resource](../../mozilla.components.browser.icons/-icon-request/-resource/index.md) for the given [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md). |

### Inheritors

| Name | Summary |
|---|---|
| [HttpIconLoader](../-http-icon-loader/index.md) | `class HttpIconLoader : `[`IconLoader`](./index.md)<br>[IconLoader](./index.md) implementation that will try to download the icon for resources that point to an http(s) URL. |
