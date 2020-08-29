[android-components](../../index.md) / [mozilla.components.browser.state.action](../index.md) / [LastAccessAction](./index.md)

# LastAccessAction

`sealed class LastAccessAction : `[`BrowserAction`](../-browser-action.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L121)

[BrowserAction](../-browser-action.md) implementations related to updating the [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) inside [BrowserState](../../mozilla.components.browser.state.state/-browser-state/index.md).

### Types

| Name | Summary |
|---|---|
| [UpdateLastAccessAction](-update-last-access-action/index.md) | `data class UpdateLastAccessAction : `[`LastAccessAction`](./index.md)<br>Updates the timestamp of the [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](-update-last-access-action/tab-id.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [UpdateLastAccessAction](-update-last-access-action/index.md) | `data class UpdateLastAccessAction : `[`LastAccessAction`](./index.md)<br>Updates the timestamp of the [TabSessionState](../../mozilla.components.browser.state.state/-tab-session-state/index.md) with the given [tabId](-update-last-access-action/tab-id.md). |
