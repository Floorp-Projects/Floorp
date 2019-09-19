[android-components](../../../index.md) / [mozilla.components.concept.engine.manifest](../../index.md) / [WebAppManifestParser](../index.md) / [Result](./index.md)

# Result

`sealed class Result` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifestParser.kt#L24)

A parsing result.

### Types

| Name | Summary |
|---|---|
| [Failure](-failure/index.md) | `data class Failure : `[`Result`](./index.md)<br>Parsing the JSON failed. |
| [Success](-success/index.md) | `data class Success : `[`Result`](./index.md)<br>The JSON was parsed successful. |

### Extension Functions

| Name | Summary |
|---|---|
| [getOrNull](../../get-or-null.md) | `fun `[`Result`](./index.md)`.getOrNull(): `[`WebAppManifest`](../../-web-app-manifest/index.md)`?`<br>Returns the encapsulated value if this instance represents success or `null` if it is failure. |

### Inheritors

| Name | Summary |
|---|---|
| [Failure](-failure/index.md) | `data class Failure : `[`Result`](./index.md)<br>Parsing the JSON failed. |
| [Success](-success/index.md) | `data class Success : `[`Result`](./index.md)<br>The JSON was parsed successful. |
