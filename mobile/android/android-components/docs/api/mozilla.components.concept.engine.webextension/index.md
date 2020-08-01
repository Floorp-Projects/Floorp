[android-components](../index.md) / [mozilla.components.concept.engine.webextension](./index.md)

## Package mozilla.components.concept.engine.webextension

### Types

| Name | Summary |
|---|---|
| [Action](-action/index.md) | `data class Action`<br>Value type that represents the state of a browser or page action within a [WebExtension](-web-extension/index.md). |
| [ActionHandler](-action-handler/index.md) | `interface ActionHandler`<br>A handler for web extension (browser and page) actions. |
| [DisabledFlags](-disabled-flags/index.md) | `class DisabledFlags`<br>Flags to check for different reasons why an extension is disabled. |
| [EnableSource](-enable-source/index.md) | `enum class EnableSource`<br>Provides additional information about why an extension is being enabled or disabled. |
| [MessageHandler](-message-handler/index.md) | `interface MessageHandler`<br>A handler for all messaging related events, usable for both content and background scripts. |
| [Metadata](-metadata/index.md) | `data class Metadata`<br>Provides information about a [WebExtension](-web-extension/index.md). |
| [Port](-port/index.md) | `abstract class Port`<br>Represents a port for exchanging messages: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port |
| [TabHandler](-tab-handler/index.md) | `interface TabHandler`<br>A handler for all tab related events (triggered by browser.tabs.* methods). |
| [WebExtension](-web-extension/index.md) | `abstract class WebExtension`<br>Represents a browser extension based on the WebExtension API: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions |
| [WebExtensionDelegate](-web-extension-delegate/index.md) | `interface WebExtensionDelegate`<br>Notifies applications or other components of engine events related to web extensions e.g. an extension was installed, or an extension wants to open a new tab. |
| [WebExtensionRuntime](-web-extension-runtime/index.md) | `interface WebExtensionRuntime`<br>Entry point for interacting with the web extensions. |

### Type Aliases

| Name | Summary |
|---|---|
| [WebExtensionBrowserAction](-web-extension-browser-action.md) | `typealias WebExtensionBrowserAction = `[`Action`](-action/index.md) |
| [WebExtensionPageAction](-web-extension-page-action.md) | `typealias WebExtensionPageAction = `[`Action`](-action/index.md) |

### Functions

| Name | Summary |
|---|---|
| [isUnsupported](is-unsupported.md) | `fun `[`WebExtension`](-web-extension/index.md)`.isUnsupported(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns whether or not the extension is disabled because it is unsupported. |
