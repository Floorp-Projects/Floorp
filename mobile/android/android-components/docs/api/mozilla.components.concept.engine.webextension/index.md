[android-components](../index.md) / [mozilla.components.concept.engine.webextension](./index.md)

## Package mozilla.components.concept.engine.webextension

### Types

| Name | Summary |
|---|---|
| [ActionHandler](-action-handler/index.md) | `interface ActionHandler`<br>A handler for web extension (browser and page) actions. |
| [BrowserAction](-browser-action/index.md) | `data class BrowserAction`<br>Value type that represents the state of a browser action within a [WebExtension](-web-extension/index.md). |
| [EnableSource](-enable-source/index.md) | `enum class EnableSource`<br>Provides additional information about why an extension was enabled or disabled. |
| [MessageHandler](-message-handler/index.md) | `interface MessageHandler`<br>A handler for all messaging related events, usable for both content and background scripts. |
| [Metadata](-metadata/index.md) | `data class Metadata`<br>Provides information about a [WebExtension](-web-extension/index.md). |
| [Port](-port/index.md) | `abstract class Port`<br>Represents a port for exchanging messages: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port |
| [WebExtension](-web-extension/index.md) | `abstract class WebExtension`<br>Represents a browser extension based on the WebExtension API: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions |
| [WebExtensionDelegate](-web-extension-delegate/index.md) | `interface WebExtensionDelegate`<br>Notifies applications or other components of engine events related to web extensions e.g. an extension was installed, or an extension wants to open a new tab. |
