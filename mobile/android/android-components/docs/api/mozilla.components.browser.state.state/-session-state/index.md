[android-components](../../index.md) / [mozilla.components.browser.state.state](../index.md) / [SessionState](./index.md)

# SessionState

`interface SessionState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/state/SessionState.kt#L21)

Interface for states that contain a [ContentState](../-content-state/index.md) and can be accessed via an [id](id.md).

### Types

| Name | Summary |
|---|---|
| [Source](-source/index.md) | `enum class Source`<br>Represents the origin of a session to describe how and why it was created. |

### Properties

| Name | Summary |
|---|---|
| [content](content.md) | `abstract val content: `[`ContentState`](../-content-state/index.md)<br>the [ContentState](../-content-state/index.md) of this session. |
| [contextId](context-id.md) | `abstract val contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>the session context ID of the session. The session context ID specifies the contextual identity to use for the session's cookie store. https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Work_with_contextual_identities |
| [engineState](engine-state.md) | `abstract val engineState: `[`EngineState`](../-engine-state/index.md)<br>the [EngineState](../-engine-state/index.md) of this session. |
| [extensionState](extension-state.md) | `abstract val extensionState: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtensionState`](../-web-extension-state/index.md)`>`<br>a map of extension id and web extension states specific to this [SessionState](./index.md). |
| [id](id.md) | `abstract val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the unique id of the session. |
| [source](source.md) | `abstract val source: `[`Source`](-source/index.md)<br>the [Source](-source/index.md) of this session to describe how and why it was created. |
| [trackingProtection](tracking-protection.md) | `abstract val trackingProtection: `[`TrackingProtectionState`](../-tracking-protection-state/index.md)<br>the [TrackingProtectionState](../-tracking-protection-state/index.md) of this session. |

### Functions

| Name | Summary |
|---|---|
| [createCopy](create-copy.md) | `abstract fun createCopy(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = this.id, content: `[`ContentState`](../-content-state/index.md)` = this.content, trackingProtection: `[`TrackingProtectionState`](../-tracking-protection-state/index.md)` = this.trackingProtection, engineState: `[`EngineState`](../-engine-state/index.md)` = this.engineState, extensionState: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`WebExtensionState`](../-web-extension-state/index.md)`> = this.extensionState, contextId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = this.contextId): `[`SessionState`](./index.md)<br>Copy the class and override some parameters. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [CustomTabSessionState](../-custom-tab-session-state/index.md) | `data class CustomTabSessionState : `[`SessionState`](./index.md)<br>Value type that represents the state of a Custom Tab. |
| [TabSessionState](../-tab-session-state/index.md) | `data class TabSessionState : `[`SessionState`](./index.md)<br>Value type that represents the state of a tab (private or normal). |
