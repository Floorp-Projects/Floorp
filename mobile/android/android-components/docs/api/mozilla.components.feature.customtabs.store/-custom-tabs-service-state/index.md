[android-components](../../index.md) / [mozilla.components.feature.customtabs.store](../index.md) / [CustomTabsServiceState](./index.md)

# CustomTabsServiceState

`data class CustomTabsServiceState : `[`State`](../../mozilla.components.lib.state/-state.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/store/CustomTabsServiceState.kt#L16)

Value type that represents the custom tabs state
accessible from both the service and activity.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `CustomTabsServiceState(tabs: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<CustomTabsSessionToken, `[`CustomTabState`](../-custom-tab-state/index.md)`> = emptyMap())`<br>Value type that represents the custom tabs state accessible from both the service and activity. |

### Properties

| Name | Summary |
|---|---|
| [tabs](tabs.md) | `val tabs: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<CustomTabsSessionToken, `[`CustomTabState`](../-custom-tab-state/index.md)`>` |
