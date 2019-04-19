[android-components](../index.md) / [mozilla.components.browser.session.action](./index.md)

## Package mozilla.components.browser.session.action

### Types

| Name | Summary |
|---|---|
| [Action](-action.md) | `interface Action`<br>Generic interface for actions to be dispatched on a [BrowserStore](../mozilla.components.browser.session.store/-browser-store/index.md). |
| [BrowserAction](-browser-action.md) | `sealed class BrowserAction : `[`Action`](-action.md)<br>[Action](-action.md) implementation related to [BrowserState](../mozilla.components.browser.session.state/-browser-state/index.md). |
| [SessionAction](-session-action/index.md) | `sealed class SessionAction : `[`BrowserAction`](-browser-action.md)<br>[BrowserAction](-browser-action.md) implementations related to updating the a single [SessionState](../mozilla.components.browser.session.state/-session-state/index.md) inside [BrowserState](../mozilla.components.browser.session.state/-browser-state/index.md). |
| [SessionListAction](-session-list-action/index.md) | `sealed class SessionListAction : `[`BrowserAction`](-browser-action.md)<br>[BrowserAction](-browser-action.md) implementations related to updating the list of [SessionState](../mozilla.components.browser.session.state/-session-state/index.md) inside [BrowserState](../mozilla.components.browser.session.state/-browser-state/index.md). |
