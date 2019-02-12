[android-components](../../index.md) / [mozilla.components.feature.tabs.tabstray](../index.md) / [TabsFeature](./index.md)

# TabsFeature

`class TabsFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/tabstray/TabsFeature.kt#L17)

Feature implementation for connecting a tabs tray implementation with the session module.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsFeature(tabsTray: `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, tabsUseCases: `[`TabsUseCases`](../../mozilla.components.feature.tabs/-tabs-use-cases/index.md)`, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Feature implementation for connecting a tabs tray implementation with the session module. |

### Functions

| Name | Summary |
|---|---|
| [filterTabs](filter-tabs.md) | `fun filterTabs(tabsFilter: (`[`Session`](../../mozilla.components.browser.session/-session/index.md)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
