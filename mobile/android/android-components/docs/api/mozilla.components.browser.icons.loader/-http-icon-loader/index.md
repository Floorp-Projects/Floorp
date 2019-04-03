[android-components](../../index.md) / [mozilla.components.browser.icons.loader](../index.md) / [HttpIconLoader](./index.md)

# HttpIconLoader

`class HttpIconLoader : `[`IconLoader`](../-icon-loader/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/loader/HttpIconLoader.kt#L17)

[IconLoader](../-icon-loader/index.md) implementation that will try to download the icon for resources that point to an http(s) URL.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `HttpIconLoader(httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`)`<br>[IconLoader](../-icon-loader/index.md) implementation that will try to download the icon for resources that point to an http(s) URL. |

### Properties

| Name | Summary |
|---|---|
| [source](source.md) | `val source: `[`Source`](../../mozilla.components.browser.icons/-icon/-source/index.md) |

### Functions

| Name | Summary |
|---|---|
| [load](load.md) | `fun load(request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`): `[`ByteArray`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-byte-array/index.html)`?`<br>Tries to load the [IconRequest.Resource](../../mozilla.components.browser.icons/-icon-request/-resource/index.md) for the given [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md). |
