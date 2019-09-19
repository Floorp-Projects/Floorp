[android-components](../index.md) / [mozilla.components.lib.state](index.md) / [Action](./-action.md)

# Action

`interface Action` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Action.kt#L14)

Generic interface for actions to be dispatched on a [Store](-store/index.md).

Actions are used to send data from the application to a [Store](-store/index.md). The [Store](-store/index.md) will use the [Action](./-action.md) to
derive a new [State](-state.md). Actions should describe what happened, while [Reducer](-reducer.md)s will describe how the
state changes.

### Inheritors

| Name | Summary |
|---|---|
| [BrowserAction](../mozilla.components.browser.state.action/-browser-action.md) | `sealed class BrowserAction : `[`Action`](./-action.md)<br>[Action](./-action.md) implementation related to [BrowserState](../mozilla.components.browser.state.state/-browser-state/index.md). |
| [CustomTabsAction](../mozilla.components.feature.customtabs.store/-custom-tabs-action/index.md) | `sealed class CustomTabsAction : `[`Action`](./-action.md) |
