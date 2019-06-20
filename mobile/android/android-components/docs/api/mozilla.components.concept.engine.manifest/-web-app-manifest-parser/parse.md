[android-components](../../index.md) / [mozilla.components.concept.engine.manifest](../index.md) / [WebAppManifestParser](index.md) / [parse](./parse.md)

# parse

`fun parse(json: `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`): `[`Result`](-result/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifestParser.kt#L39)

Parses the provided JSON and returns a [WebAppManifest](../-web-app-manifest/index.md) (wrapped in [Result.Success](-result/-success/index.md) if parsing was successful.
Otherwise [Result.Failure](-result/-failure/index.md).

