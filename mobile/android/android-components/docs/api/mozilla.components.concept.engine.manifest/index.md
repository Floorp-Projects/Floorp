[android-components](../index.md) / [mozilla.components.concept.engine.manifest](./index.md)

## Package mozilla.components.concept.engine.manifest

### Types

| Name | Summary |
|---|---|
| [Size](-size/index.md) | `data class Size`<br>Represents dimensions for an image. Corresponds to values of the "sizes" HTML attribute. |
| [WebAppManifest](-web-app-manifest/index.md) | `data class WebAppManifest`<br>The web app manifest provides information about an application (such as its name, author, icon, and description). |
| [WebAppManifestParser](-web-app-manifest-parser/index.md) | `class WebAppManifestParser`<br>Parser for constructing a [WebAppManifest](-web-app-manifest/index.md) from JSON. |

### Functions

| Name | Summary |
|---|---|
| [getOrNull](get-or-null.md) | `fun `[`Result`](-web-app-manifest-parser/-result/index.md)`.getOrNull(): `[`WebAppManifest`](-web-app-manifest/index.md)`?`<br>Returns the encapsulated value if this instance represents success or `null` if it is failure. |
