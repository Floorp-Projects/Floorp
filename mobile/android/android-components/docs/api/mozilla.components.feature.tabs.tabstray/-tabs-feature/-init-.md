[android-components](../../index.md) / [mozilla.components.feature.tabs.tabstray](../index.md) / [TabsFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`TabsFeature(tabsTray: `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, selectTabUseCase: `[`SelectTabUseCase`](../../mozilla.components.feature.tabs/-tabs-use-cases/-select-tab-use-case/index.md)`, removeTabUseCase: `[`RemoveTabUseCase`](../../mozilla.components.feature.tabs/-tabs-use-cases/-remove-tab-use-case/index.md)`, defaultTabsFilter: (`[`TabSessionState`](../../mozilla.components.browser.state.state/-tab-session-state/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = { true }, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

Feature implementation for connecting a tabs tray implementation with the session module.

### Parameters

`defaultTabsFilter` - A tab filter that is used for the initial presenting of tabs that will be used by
[TabsFeature.filterTabs](filter-tabs.md) by default as well.