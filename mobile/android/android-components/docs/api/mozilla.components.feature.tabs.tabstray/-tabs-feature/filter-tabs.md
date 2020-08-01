[android-components](../../index.md) / [mozilla.components.feature.tabs.tabstray](../index.md) / [TabsFeature](index.md) / [filterTabs](./filter-tabs.md)

# filterTabs

`fun filterTabs(tabsFilter: (`[`TabSessionState`](../../mozilla.components.browser.state.state/-tab-session-state/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = defaultTabsFilter): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/tabstray/TabsFeature.kt#L61)

Filter the list of tabs using [tabsFilter](filter-tabs.md#mozilla.components.feature.tabs.tabstray.TabsFeature$filterTabs(kotlin.Function1((mozilla.components.browser.state.state.TabSessionState, kotlin.Boolean)))/tabsFilter).

### Parameters

`tabsFilter` - A filter function returning `true` for all tabs that should be displayed in
the tabs tray. Uses the [defaultTabsFilter](#) if none is provided.