[android-components](../index.md) / [mozilla.components.browser.state.action](index.md) / [BrowserAction](./-browser-action.md)

# BrowserAction

`sealed class BrowserAction : `[`Action`](../mozilla.components.lib.state/-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L41)

[Action](../mozilla.components.lib.state/-action.md) implementation related to [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md).

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [ContainerAction](-container-action/index.md) | `sealed class ContainerAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating [BrowserState.containers](../mozilla.components.browser.state.state/-browser-state/containers.md) |
| [ContentAction](-content-action/index.md) | `sealed class ContentAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the [ContentState](../mozilla.components.browser.state.state/-content-state/index.md) of a single [SessionState](../mozilla.components.browser.state.state/-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [CustomTabListAction](-custom-tab-list-action/index.md) | `sealed class CustomTabListAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating [BrowserState.customTabs](../mozilla.components.browser.state.state/-browser-state/custom-tabs.md). |
| [DownloadAction](-download-action/index.md) | `sealed class DownloadAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the global download state. |
| [EngineAction](-engine-action/index.md) | `sealed class EngineAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the [EngineState](../mozilla.components.browser.state.state/-engine-state/index.md) of a single [SessionState](../mozilla.components.browser.state.state/-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [MediaAction](-media-action/index.md) | `sealed class MediaAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the [MediaState](../mozilla.components.browser.state.state/-media-state/index.md). |
| [ReaderAction](-reader-action/index.md) | `sealed class ReaderAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the [ReaderState](../mozilla.components.browser.state.state/-reader-state/index.md) of a single [TabSessionState](../mozilla.components.browser.state.state/-tab-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [SearchAction](-search-action/index.md) | `sealed class SearchAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating search engines in [SearchState](../mozilla.components.browser.state.state/-search-state/index.md). |
| [SystemAction](-system-action/index.md) | `sealed class SystemAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations to react to system events. |
| [TabListAction](-tab-list-action/index.md) | `sealed class TabListAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the list of [TabSessionState](../mozilla.components.browser.state.state/-tab-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [TrackingProtectionAction](-tracking-protection-action/index.md) | `sealed class TrackingProtectionAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the [TrackingProtectionState](../mozilla.components.browser.state.state/-tracking-protection-state/index.md) of a single [SessionState](../mozilla.components.browser.state.state/-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [WebExtensionAction](-web-extension-action/index.md) | `sealed class WebExtensionAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating [BrowserState.extensions](../mozilla.components.browser.state.state/-browser-state/extensions.md) and [TabSessionState.extensionState](../mozilla.components.browser.state.state/-tab-session-state/extension-state.md). |
