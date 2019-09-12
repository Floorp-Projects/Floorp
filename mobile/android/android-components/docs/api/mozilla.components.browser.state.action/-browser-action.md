[android-components](../index.md) / [mozilla.components.browser.state.action](index.md) / [BrowserAction](./-browser-action.md)

# BrowserAction

`sealed class BrowserAction : `[`Action`](../mozilla.components.lib.state/-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L28)

[Action](../mozilla.components.lib.state/-action.md) implementation related to [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md).

### Inheritors

| Name | Summary |
|---|---|
| [ContentAction](-content-action/index.md) | `sealed class ContentAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the [ContentState](../mozilla.components.browser.state.state/-content-state/index.md) of a single [SessionState](../mozilla.components.browser.state.state/-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [CustomTabListAction](-custom-tab-list-action/index.md) | `sealed class CustomTabListAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating [BrowserState.customTabs](../mozilla.components.browser.state.state/-browser-state/custom-tabs.md). |
| [EngineAction](-engine-action/index.md) | `sealed class EngineAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the [EngineState](../mozilla.components.browser.state.state/-engine-state/index.md) of a single [SessionState](../mozilla.components.browser.state.state/-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [SystemAction](-system-action/index.md) | `sealed class SystemAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations to react to system events. |
| [TabListAction](-tab-list-action/index.md) | `sealed class TabListAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the list of [TabSessionState](../mozilla.components.browser.state.state/-tab-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [TrackingProtectionAction](-tracking-protection-action/index.md) | `sealed class TrackingProtectionAction : `[`BrowserAction`](./-browser-action.md)<br>[BrowserAction](./-browser-action.md) implementations related to updating the [TrackingProtectionState](../mozilla.components.browser.state.state/-tracking-protection-state/index.md) of a single [SessionState](../mozilla.components.browser.state.state/-session-state/index.md) inside [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
