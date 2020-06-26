[android-components](../../index.md) / [mozilla.components.feature.tabs.tabstray](../index.md) / [TabsFeature](./index.md)

# TabsFeature

`class TabsFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/tabstray/TabsFeature.kt#L22)

Feature implementation for connecting a tabs tray implementation with the session module.

### Parameters

`defaultTabsFilter` - A tab filter that is used for the initial presenting of tabs that will be used by
[TabsFeature.filterTabs](filter-tabs.md) by default as well.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsFeature(tabsTray: `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, selectTabUseCase: `[`SelectTabUseCase`](../../mozilla.components.feature.tabs/-tabs-use-cases/-select-tab-use-case/index.md)`, removeTabUseCase: `[`RemoveTabUseCase`](../../mozilla.components.feature.tabs/-tabs-use-cases/-remove-tab-use-case/index.md)`, defaultTabsFilter: (`[`TabSessionState`](../../mozilla.components.browser.state.state/-tab-session-state/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Feature implementation for connecting a tabs tray implementation with the session module. |

### Functions

| Name | Summary |
|---|---|
| [filterTabs](filter-tabs.md) | `fun filterTabs(tabsFilter: (`[`TabSessionState`](../../mozilla.components.browser.state.state/-tab-session-state/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = defaultTabsFilter): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Filter the list of tabs using [tabsFilter](filter-tabs.md#mozilla.components.feature.tabs.tabstray.TabsFeature$filterTabs(kotlin.Function1((mozilla.components.browser.state.state.TabSessionState, kotlin.Boolean)))/tabsFilter). |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
