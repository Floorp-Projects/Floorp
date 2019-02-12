[android-components](../../../index.md) / [mozilla.components.concept.tabstray](../../index.md) / [TabsTray](../index.md) / [Observer](./index.md)

# Observer

`interface Observer` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/tabstray/src/main/java/mozilla/components/concept/tabstray/TabsTray.kt#L18)

Interface to be implemented by classes that want to observe a tabs tray.

### Functions

| Name | Summary |
|---|---|
| [onTabClosed](on-tab-closed.md) | `abstract fun onTabClosed(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A tab has been closed. |
| [onTabSelected](on-tab-selected.md) | `abstract fun onTabSelected(session: `[`Session`](../../../mozilla.components.browser.session/-session/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>A new tab has been selected. |

### Inheritors

| Name | Summary |
|---|---|
| [TabsTrayInteractor](../../../mozilla.components.feature.tabs.tabstray/-tabs-tray-interactor/index.md) | `class TabsTrayInteractor : `[`Observer`](./index.md)<br>Interactor for a tabs tray component. Subscribes to the tabs tray and invokes use cases to update the session manager. |
