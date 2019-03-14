[android-components](../../index.md) / [mozilla.components.browser.session.manifest](../index.md) / [WebAppManifestParser](index.md) / [parse](./parse.md)

# parse

`fun parse(json: `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`): `[`Result`](-result/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/manifest/WebAppManifestParser.kt#L37)

Parses the provided JSON and returns a [WebAppManifest](../-web-app-manifest/index.md) (wrapped in [Result.Success](-result/-success/index.md) if parsing was successful.
Otherwise [Result.Failure](-result/-failure/index.md).

