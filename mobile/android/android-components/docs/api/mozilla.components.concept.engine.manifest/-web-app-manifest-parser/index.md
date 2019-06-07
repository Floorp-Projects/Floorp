[android-components](../../index.md) / [mozilla.components.concept.engine.manifest](../index.md) / [WebAppManifestParser](./index.md)

# WebAppManifestParser

`class WebAppManifestParser` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifestParser.kt#L14)

Parser for constructing a [WebAppManifest](../-web-app-manifest/index.md) from JSON.

### Types

| Name | Summary |
|---|---|
| [Result](-result/index.md) | `sealed class Result`<br>A parsing result. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppManifestParser()`<br>Parser for constructing a [WebAppManifest](../-web-app-manifest/index.md) from JSON. |

### Functions

| Name | Summary |
|---|---|
| [parse](parse.md) | `fun parse(json: `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`): `[`Result`](-result/index.md)<br>Parses the provided JSON and returns a [WebAppManifest](../-web-app-manifest/index.md) (wrapped in [Result.Success](-result/-success/index.md) if parsing was successful. Otherwise [Result.Failure](-result/-failure/index.md). |
