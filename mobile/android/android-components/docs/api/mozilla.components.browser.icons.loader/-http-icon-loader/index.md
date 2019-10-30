[android-components](../../index.md) / [mozilla.components.browser.icons.loader](../index.md) / [HttpIconLoader](./index.md)

# HttpIconLoader

`class HttpIconLoader : `[`IconLoader`](../-icon-loader/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/loader/HttpIconLoader.kt#L29)

[IconLoader](../-icon-loader/index.md) implementation that will try to download the icon for resources that point to an http(s) URL.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `HttpIconLoader(httpClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`)`<br>[IconLoader](../-icon-loader/index.md) implementation that will try to download the icon for resources that point to an http(s) URL. |

### Functions

| Name | Summary |
|---|---|
| [load](load.md) | `fun load(context: <ERROR CLASS>, request: `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)`, resource: `[`Resource`](../../mozilla.components.browser.icons/-icon-request/-resource/index.md)`): `[`Result`](../-icon-loader/-result/index.md)<br>Tries to load the [IconRequest.Resource](../../mozilla.components.browser.icons/-icon-request/-resource/index.md) for the given [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
