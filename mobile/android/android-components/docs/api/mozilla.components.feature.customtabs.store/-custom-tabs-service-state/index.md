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

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
