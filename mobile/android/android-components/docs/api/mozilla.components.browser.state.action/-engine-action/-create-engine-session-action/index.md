[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [EngineAction](../index.md) / [CreateEngineSessionAction](./index.md)

# CreateEngineSessionAction

`data class CreateEngineSessionAction : `[`EngineAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L458)

Creates an [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) for the given [tabId](tab-id.md) if none exists yet.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CreateEngineSessionAction(tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, skipLoading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>Creates an [EngineSession](../../../mozilla.components.concept.engine/-engine-session/index.md) for the given [tabId](tab-id.md) if none exists yet. |

### Properties

| Name | Summary |
|---|---|
| [skipLoading](skip-loading.md) | `val skipLoading: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [tabId](tab-id.md) | `val tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
