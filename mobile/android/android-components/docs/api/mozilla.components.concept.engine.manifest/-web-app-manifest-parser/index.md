[android-components](../../index.md) / [mozilla.components.concept.engine.manifest](../index.md) / [WebAppManifestParser](./index.md)

# WebAppManifestParser

`class WebAppManifestParser` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifestParser.kt#L24)

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
| [parse](parse.md) | `fun parse(json: <ERROR CLASS>): `[`Result`](-result/index.md)<br>`fun parse(json: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Failure`](-result/-failure/index.md)<br>Parses the provided JSON and returns a [WebAppManifest](../-web-app-manifest/index.md) (wrapped in [Result.Success](-result/-success/index.md) if parsing was successful. Otherwise [Result.Failure](-result/-failure/index.md). |
| [serialize](serialize.md) | `fun serialize(manifest: `[`WebAppManifest`](../-web-app-manifest/index.md)`): <ERROR CLASS>` |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
