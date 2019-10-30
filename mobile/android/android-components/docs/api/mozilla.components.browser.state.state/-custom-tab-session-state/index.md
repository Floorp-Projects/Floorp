[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [CustomTabSessionState](./index.md)

# CustomTabSessionState

`data class CustomTabSessionState : `[`SessionState`](../-session-state/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/CustomTabSessionState.kt#L19)

Value type that represents the state of a Custom Tab.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomTabSessionState(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString(), content: `[`ContentState`](../-content-state/index.md)`, trackingProtection: `[`TrackingProtectionState`](../-tracking-protection-state/index.md)` = TrackingProtectionState(), config: `[`CustomTabConfig`](../-custom-tab-config/index.md)`, engineState: `[`EngineState`](../-engine-state/index.md)` = EngineState(), extensionState: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtensionState`](../-web-extension-state/index.md)`> = emptyMap())`<br>Value type that represents the state of a Custom Tab. |

### Properties

| Name | Summary |
|---|---|
| [config](config.md) | `val config: `[`CustomTabConfig`](../-custom-tab-config/index.md)<br>the [CustomTabConfig](../-custom-tab-config/index.md) used to create this custom tab. |
| [content](content.md) | `val content: `[`ContentState`](../-content-state/index.md)<br>the [ContentState](../-content-state/index.md) of this custom tab. |
| [engineState](engine-state.md) | `val engineState: `[`EngineState`](../-engine-state/index.md)<br>the [EngineState](../-engine-state/index.md) of this session. |
| [extensionState](extension-state.md) | `val extensionState: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtensionState`](../-web-extension-state/index.md)`>`<br>a map of web extension ids and extensions, that contains the overridden values for this tab. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the ID of this custom tab and session. |
| [trackingProtection](tracking-protection.md) | `val trackingProtection: `[`TrackingProtectionState`](../-tracking-protection-state/index.md)<br>the [TrackingProtectionState](../-tracking-protection-state/index.md) of this custom tab. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
