---
title: TabsTrayInteractor - 
---

[mozilla.components.feature.tabs.tabstray](../index.html) / [TabsTrayInteractor](./index.html)

# TabsTrayInteractor

`class TabsTrayInteractor : Observer`

Interactor for a tabs tray component. Subscribes to the tabs tray and invokes use cases to update
the session manager.

### Constructors

| [&lt;init&gt;](-init-.html) | `TabsTrayInteractor(tabsTray: TabsTray, selectTabUseCase: `[`SelectTabUseCase`](../../mozilla.components.feature.tabs/-tabs-use-cases/-select-tab-use-case/index.html)`, removeTabUseCase: `[`RemoveTabUseCase`](../../mozilla.components.feature.tabs/-tabs-use-cases/-remove-tab-use-case/index.html)`, closeTabsTray: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Interactor for a tabs tray component. Subscribes to the tabs tray and invokes use cases to update the session manager. |

### Functions

| [onTabClosed](on-tab-closed.html) | `fun onTabClosed(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onTabSelected](on-tab-selected.html) | `fun onTabSelected(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [start](start.html) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.html) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

