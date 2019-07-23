[android-components](../../index.md) / [mozilla.components.feature.tabs](../index.md) / [TabsUseCases](./index.md)

# TabsUseCases

`class TabsUseCases` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt#L16)

Contains use cases related to the tabs feature.

### Types

| Name | Summary |
|---|---|
| [AddNewPrivateTabUseCase](-add-new-private-tab-use-case/index.md) | `class AddNewPrivateTabUseCase : `[`LoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-load-url-use-case/index.md) |
| [AddNewTabUseCase](-add-new-tab-use-case/index.md) | `class AddNewTabUseCase : `[`LoadUrlUseCase`](../../mozilla.components.feature.session/-session-use-cases/-load-url-use-case/index.md) |
| [DefaultSelectTabUseCase](-default-select-tab-use-case/index.md) | `class DefaultSelectTabUseCase : `[`SelectTabUseCase`](-select-tab-use-case/index.md) |
| [RemoveAllTabsOfTypeUseCase](-remove-all-tabs-of-type-use-case/index.md) | `class RemoveAllTabsOfTypeUseCase` |
| [RemoveAllTabsUseCase](-remove-all-tabs-use-case/index.md) | `class RemoveAllTabsUseCase` |
| [RemoveTabUseCase](-remove-tab-use-case/index.md) | `class RemoveTabUseCase` |
| [SelectTabUseCase](-select-tab-use-case/index.md) | `interface SelectTabUseCase`<br>Contract for use cases that select a tab. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsUseCases(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`)`<br>Contains use cases related to the tabs feature. |

### Properties

| Name | Summary |
|---|---|
| [addPrivateTab](add-private-tab.md) | `val addPrivateTab: `[`AddNewPrivateTabUseCase`](-add-new-private-tab-use-case/index.md) |
| [addTab](add-tab.md) | `val addTab: `[`AddNewTabUseCase`](-add-new-tab-use-case/index.md) |
| [removeAllTabs](remove-all-tabs.md) | `val removeAllTabs: `[`RemoveAllTabsUseCase`](-remove-all-tabs-use-case/index.md) |
| [removeAllTabsOfType](remove-all-tabs-of-type.md) | `val removeAllTabsOfType: `[`RemoveAllTabsOfTypeUseCase`](-remove-all-tabs-of-type-use-case/index.md) |
| [removeTab](remove-tab.md) | `val removeTab: `[`RemoveTabUseCase`](-remove-tab-use-case/index.md) |
| [selectTab](select-tab.md) | `val selectTab: `[`SelectTabUseCase`](-select-tab-use-case/index.md) |
