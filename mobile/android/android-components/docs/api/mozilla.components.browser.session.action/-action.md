[android-components](../index.md) / [mozilla.components.browser.session.action](index.md) / [Action](./-action.md)

# Action

`interface Action` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/action/Action.kt#L16)

Generic interface for actions to be dispatched on a [BrowserStore](../mozilla.components.browser.session.store/-browser-store/index.md).

Actions are used to send data from the application to a [BrowserStore](../mozilla.components.browser.session.store/-browser-store/index.md). The [BrowserStore](../mozilla.components.browser.session.store/-browser-store/index.md) will use the [Action](./-action.md) to
derive a new [BrowserState](../mozilla.components.browser.session.state/-browser-state/index.md).

### Inheritors

| Name | Summary |
|---|---|
| [BrowserAction](-browser-action.md) | `sealed class BrowserAction : `[`Action`](./-action.md)<br>[Action](./-action.md) implementation related to [BrowserState](../mozilla.components.browser.session.state/-browser-state/index.md). |
