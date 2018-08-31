---
title: TabsTray.Observer - 
---

[mozilla.components.concept.tabstray](../../index.html) / [TabsTray](../index.html) / [Observer](./index.html)

# Observer

`interface Observer`

Interface to be implemented by classes that want to observe a tabs tray.

### Functions

| [onTabClosed](on-tab-closed.html) | `abstract fun onTabClosed(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A tab has been closed. |
| [onTabSelected](on-tab-selected.html) | `abstract fun onTabSelected(session: Session): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new tab has been selected. |

