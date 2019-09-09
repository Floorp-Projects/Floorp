[android-components](../index.md) / [mozilla.components.browser.state.state](./index.md)

## Package mozilla.components.browser.state.state

### Types

| Name | Summary |
|---|---|
| [BrowserState](-browser-state/index.md) | `data class BrowserState : `[`State`](../mozilla.components.lib.state/-state.md)<br>Value type that represents the complete state of the browser/engine. |
| [ContentState](-content-state/index.md) | `data class ContentState`<br>Value type that represents the state of the content within a [SessionState](-session-state/index.md). |
| [CustomTabActionButtonConfig](-custom-tab-action-button-config/index.md) | `data class CustomTabActionButtonConfig` |
| [CustomTabConfig](-custom-tab-config/index.md) | `data class CustomTabConfig`<br>Holds configuration data for a Custom Tab. |
| [CustomTabMenuItem](-custom-tab-menu-item/index.md) | `data class CustomTabMenuItem` |
| [CustomTabSessionState](-custom-tab-session-state/index.md) | `data class CustomTabSessionState : `[`SessionState`](-session-state/index.md)<br>Value type that represents the state of a Custom Tab. |
| [EngineState](-engine-state/index.md) | `data class EngineState`<br>Value type that holds the browser engine state of a session. |
| [SecurityInfoState](-security-info-state/index.md) | `data class SecurityInfoState`<br>A value type holding security information for a Session. |
| [SessionState](-session-state/index.md) | `interface SessionState`<br>Interface for states that contain a [ContentState](-content-state/index.md) and can be accessed via an [id](-session-state/id.md). |
| [TabSessionState](-tab-session-state/index.md) | `data class TabSessionState : `[`SessionState`](-session-state/index.md)<br>Value type that represents the state of a tab (private or normal). |
| [TrackingProtectionState](-tracking-protection-state/index.md) | `data class TrackingProtectionState`<br>Value type that represents the state of tracking protection within a [SessionState](-session-state/index.md). |

### Functions

| Name | Summary |
|---|---|
| [createCustomTab](create-custom-tab.md) | `fun createCustomTab(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), config: `[`CustomTabConfig`](-custom-tab-config/index.md)` = CustomTabConfig()): `[`CustomTabSessionState`](-custom-tab-session-state/index.md) |
| [createTab](create-tab.md) | `fun createTab(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, private: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), parent: `[`TabSessionState`](-tab-session-state/index.md)`? = null): `[`TabSessionState`](-tab-session-state/index.md) |
