[android-components](../../index.md) / [mozilla.components.concept.tabstray](../index.md) / [TabsTray](./index.md)

# TabsTray

`interface TabsTray : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/tabstray/src/main/java/mozilla/components/concept/tabstray/TabsTray.kt#L13)

Generic interface for components that provide "tabs tray" functionality.

### Types

| Name | Summary |
|---|---|
| [Observer](-observer/index.md) | `interface Observer`<br>Interface to be implemented by classes that want to observe a tabs tray. |

### Functions

| Name | Summary |
|---|---|
| [asView](as-view.md) | `open fun asView(): <ERROR CLASS>`<br>Convenience method to cast the implementation of this interface to an Android View object. |
| [onTabsChanged](on-tabs-changed.md) | `abstract fun onTabsChanged(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateTabs() when count number of tabs are updated at the given position. |
| [onTabsInserted](on-tabs-inserted.md) | `abstract fun onTabsInserted(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateTabs() when count number of tabs are inserted at the given position. |
| [onTabsMoved](on-tabs-moved.md) | `abstract fun onTabsMoved(fromPosition: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, toPosition: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateTabs() when a tab changes it position. |
| [onTabsRemoved](on-tabs-removed.md) | `abstract fun onTabsRemoved(position: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, count: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Called after updateTabs() when count number of tabs are removed from the given position. |
| [updateTabs](update-tabs.md) | `abstract fun updateTabs(tabs: `[`Tabs`](../-tabs/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Updates the list of tabs. |

### Inherited Functions

| Name | Summary |
|---|---|
| [isObserved](../../mozilla.components.support.base.observer/-observable/is-observed.md) | `abstract fun isObserved(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If the observable has registered observers. |
| [notifyAtLeastOneObserver](../../mozilla.components.support.base.observer/-observable/notify-at-least-one-observer.md) | `abstract fun notifyAtLeastOneObserver(block: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers about a change. If there is no observer the notification is queued and sent to the first observer that is registered. |
| [notifyObservers](../../mozilla.components.support.base.observer/-observable/notify-observers.md) | `abstract fun notifyObservers(block: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers about a change. |
| [pauseObserver](../../mozilla.components.support.base.observer/-observable/pause-observer.md) | `abstract fun pauseObserver(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Pauses the provided observer. No notifications will be sent to this observer until [resumeObserver](../../mozilla.components.support.base.observer/-observable/resume-observer.md) is called. |
| [register](../../mozilla.components.support.base.observer/-observable/register.md) | `abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`, view: <ERROR CLASS>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. |
| [resumeObserver](../../mozilla.components.support.base.observer/-observable/resume-observer.md) | `abstract fun resumeObserver(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resumes the provided observer. Notifications sent since it was last paused (see [pauseObserver](../../mozilla.components.support.base.observer/-observable/pause-observer.md)]) are lost and will not be re-delivered. |
| [unregister](../../mozilla.components.support.base.observer/-observable/unregister.md) | `abstract fun unregister(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer. |
| [unregisterObservers](../../mozilla.components.support.base.observer/-observable/unregister-observers.md) | `abstract fun unregisterObservers(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters all observers. |
| [wrapConsumers](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md) | `abstract fun <R> wrapConsumers(block: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`.(`[`R`](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`R`](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Returns a list of lambdas wrapping a consuming method of an observer. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [BrowserTabsTray](../../mozilla.components.browser.tabstray/-browser-tabs-tray/index.md) | `class ~~BrowserTabsTray~~ : RecyclerView, `[`TabsTray`](./index.md)<br>A customizable tabs tray for browsers. |
| [TabsAdapter](../../mozilla.components.browser.tabstray/-tabs-adapter/index.md) | `open class TabsAdapter : Adapter<`[`TabViewHolder`](../../mozilla.components.browser.tabstray/-tab-view-holder/index.md)`>, `[`TabsTray`](./index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>`<br>RecyclerView adapter implementation to display a list/grid of tabs. |
