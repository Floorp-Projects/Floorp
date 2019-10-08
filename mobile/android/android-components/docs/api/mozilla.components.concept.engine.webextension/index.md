[android-components](../index.md) / [mozilla.components.concept.engine.webextension](./index.md)

## Package mozilla.components.concept.engine.webextension

### Types

| Name | Summary |
|---|---|
| [MessageHandler](-message-handler/index.md) | `interface MessageHandler`<br>A handler for all messaging related events, usable for both content and background scripts. |
| [Port](-port/index.md) | `abstract class Port`<br>Represents a port for exchanging messages: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port |
| [WebExtension](-web-extension/index.md) | `abstract class WebExtension`<br>Represents a browser extension based on the WebExtension API: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions |
| [WebExtensionTabDelegate](-web-extension-tab-delegate/index.md) | `interface WebExtensionTabDelegate`<br>Notifies applications / other components that a web extension wants to open a new tab via browser.tabs.create. Note that browser.tabs.remove is currently not supported: https://github.com/mozilla-mobile/android-components/issues/4682 |
