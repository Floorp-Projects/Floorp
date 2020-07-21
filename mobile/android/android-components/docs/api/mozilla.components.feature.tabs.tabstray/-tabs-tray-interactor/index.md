[android-components](../../index.md) / [mozilla.components.feature.tabs.tabstray](../index.md) / [TabsTrayInteractor](./index.md)

# TabsTrayInteractor

`class TabsTrayInteractor : `[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/tabstray/TabsTrayInteractor.kt#L15)

Interactor for a tabs tray component. Subscribes to the tabs tray and invokes use cases to update
the session manager.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TabsTrayInteractor(tabsTray: `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, selectTabUseCase: `[`SelectTabUseCase`](../../mozilla.components.feature.tabs/-tabs-use-cases/-select-tab-use-case/index.md)`, removeTabUseCase: `[`RemoveTabUseCase`](../../mozilla.components.feature.tabs/-tabs-use-cases/-remove-tab-use-case/index.md)`, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Interactor for a tabs tray component. Subscribes to the tabs tray and invokes use cases to update the session manager. |

### Functions

| Name | Summary |
|---|---|
| [onTabClosed](on-tab-closed.md) | `fun onTabClosed(tab: `[`Tab`](../../mozilla.components.concept.tabstray/-tab/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A tab has been closed. |
| [onTabSelected](on-tab-selected.md) | `fun onTabSelected(tab: `[`Tab`](../../mozilla.components.concept.tabstray/-tab/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new tab has been selected. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
