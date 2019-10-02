[android-components](../index.md) / [mozilla.components.feature.customtabs](./index.md)

## Package mozilla.components.feature.customtabs

### Types

| Name | Summary |
|---|---|
| [AbstractCustomTabsService](-abstract-custom-tabs-service/index.md) | `abstract class AbstractCustomTabsService : CustomTabsService`<br>[Service](#) providing Custom Tabs related functionality. |
| [CustomTabIntentProcessor](-custom-tab-intent-processor/index.md) | `class CustomTabIntentProcessor : `[`IntentProcessor`](../mozilla.components.feature.intent.processing/-intent-processor/index.md)<br>Processor for intents which trigger actions related to custom tabs. |
| [CustomTabWindowFeature](-custom-tab-window-feature/index.md) | `class CustomTabWindowFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation for handling window requests by opening custom tabs. |
| [CustomTabsFacts](-custom-tabs-facts/index.md) | `class CustomTabsFacts`<br>Facts emitted for telemetry related to [CustomTabsToolbarFeature](-custom-tabs-toolbar-feature/index.md) |
| [CustomTabsToolbarFeature](-custom-tabs-toolbar-feature/index.md) | `class CustomTabsToolbarFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../mozilla.components.support.base.feature/-back-handler/index.md)<br>Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig. |

### Functions

| Name | Summary |
|---|---|
| [createCustomTabConfigFromIntent](create-custom-tab-config-from-intent.md) | `fun createCustomTabConfigFromIntent(intent: <ERROR CLASS>, resources: <ERROR CLASS>?): `[`CustomTabConfig`](../mozilla.components.browser.state.state/-custom-tab-config/index.md)<br>Creates a [CustomTabConfig](../mozilla.components.browser.state.state/-custom-tab-config/index.md) instance based on the provided intent. |
| [isCustomTabIntent](is-custom-tab-intent.md) | `fun isCustomTabIntent(intent: <ERROR CLASS>): <ERROR CLASS>`<br>`fun isCustomTabIntent(safeIntent: `[`SafeIntent`](../mozilla.components.support.utils/-safe-intent/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the provided intent is a custom tab intent. |
| [isTrustedWebActivityIntent](is-trusted-web-activity-intent.md) | `fun isTrustedWebActivityIntent(intent: <ERROR CLASS>): <ERROR CLASS>`<br>`fun isTrustedWebActivityIntent(safeIntent: `[`SafeIntent`](../mozilla.components.support.utils/-safe-intent/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Checks if the provided intent is a trusted web activity intent. |
