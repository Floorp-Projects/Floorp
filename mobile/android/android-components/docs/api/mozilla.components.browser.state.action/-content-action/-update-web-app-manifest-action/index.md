[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [UpdateWebAppManifestAction](./index.md)

# UpdateWebAppManifestAction

`data class UpdateWebAppManifestAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L294)

Updates the [WebAppManifest](../../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UpdateWebAppManifestAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, webAppManifest: `[`WebAppManifest`](../../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`)`<br>Updates the [WebAppManifest](../../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md) of the [ContentState](../../../mozilla.components.browser.state.state/-content-state/index.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [webAppManifest](web-app-manifest.md) | `val webAppManifest: `[`WebAppManifest`](../../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md) |
