[android-components](../../../index.md) / [mozilla.components.concept.engine.manifest](../../index.md) / [WebAppManifest](../index.md) / [ShareTarget](./index.md)

# ShareTarget

`data class ShareTarget` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifest.kt#L205)

Used to define how the web app receives share data.
If present, a share target should be created so that other Android apps can share to this web app.

### Types

| Name | Summary |
|---|---|
| [EncodingType](-encoding-type/index.md) | `enum class EncodingType`<br>Valid encoding MIME types for [ShareTarget.encType](enc-type.md). |
| [Files](-files/index.md) | `data class Files`<br>Specifies a form field member used to share files. |
| [Params](-params/index.md) | `data class Params`<br>Specifies what query parameters correspond to share data. |
| [RequestMethod](-request-method/index.md) | `enum class RequestMethod`<br>Valid HTTP methods for [ShareTarget.method](method.md). |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ShareTarget(action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, method: `[`RequestMethod`](-request-method/index.md)` = RequestMethod.GET, encType: `[`EncodingType`](-encoding-type/index.md)` = EncodingType.URL_ENCODED, params: `[`Params`](-params/index.md)` = Params())`<br>Used to define how the web app receives share data. If present, a share target should be created so that other Android apps can share to this web app. |

### Properties

| Name | Summary |
|---|---|
| [action](action.md) | `val action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>URL to open on share |
| [encType](enc-type.md) | `val encType: `[`EncodingType`](-encoding-type/index.md)<br>MIME type to specify how the params are encoded. |
| [method](method.md) | `val method: `[`RequestMethod`](-request-method/index.md)<br>Method to use with [action](action.md). Either "GET" or "POST". |
| [params](params.md) | `val params: `[`Params`](-params/index.md)<br>Specifies what query parameters correspond to share data. |
