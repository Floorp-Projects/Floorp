[android-components](../../index.md) / [mozilla.components.concept.engine.manifest](../index.md) / [WebAppManifestParser](index.md) / [parse](./parse.md)

# parse

`fun parse(json: <ERROR CLASS>): `[`Result`](-result/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifestParser.kt#L58)

Parses the provided JSON and returns a [WebAppManifest](../-web-app-manifest/index.md) (wrapped in [Result.Success](-result/-success/index.md) if parsing was successful.
Otherwise [Result.Failure](-result/-failure/index.md).

Gecko performs some initial parsing on the Web App Manifest, so the [JSONObject](#) we work with
does not match what was originally provided by the website. Gecko:

* Changes relative URLs to be absolute
* Changes some space-separated strings into arrays (purpose, sizes)
* Changes colors to follow Android format (#AARRGGBB)
* Removes invalid enum values (ie display: halfscreen)
* Ensures display, dir, start_url, and scope always have a value
* Trims most strings (name, short_name, ...)
See https://searchfox.org/mozilla-central/source/dom/manifest/ManifestProcessor.jsm
`fun parse(json: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Failure`](-result/-failure/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifestParser.kt#L90)

Parses the provided JSON and returns a [WebAppManifest](../-web-app-manifest/index.md) (wrapped in [Result.Success](-result/-success/index.md) if parsing was successful.
Otherwise [Result.Failure](-result/-failure/index.md).

