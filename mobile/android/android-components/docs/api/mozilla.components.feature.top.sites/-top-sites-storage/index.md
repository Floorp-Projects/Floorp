[android-components](../../index.md) / [mozilla.components.feature.top.sites](../index.md) / [TopSitesStorage](./index.md)

# TopSitesStorage

`interface TopSitesStorage : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/top-sites/src/main/java/mozilla/components/feature/top/sites/TopSitesStorage.kt#L12)

Abstraction layer above the [PinnedSiteStorage](../-pinned-site-storage/index.md) and [PlacesHistoryStorage](#) storages.

### Types

| Name | Summary |
|---|---|
| [Observer](-observer/index.md) | `interface Observer`<br>Interface to be implemented by classes that want to observe the top site storage. |

### Functions

| Name | Summary |
|---|---|
| [addTopSite](add-top-site.md) | `abstract fun addTopSite(title: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isDefault: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Adds a new top site. |
| [getTopSites](get-top-sites.md) | `abstract suspend fun getTopSites(totalSites: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, includeFrecent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`TopSite`](../-top-site/index.md)`>`<br>Return a unified list of top sites based on the given number of sites desired. If `includeFrecent` is true, fill in any missing top sites with frecent top site results. |
| [removeTopSite](remove-top-site.md) | `abstract fun removeTopSite(topSite: `[`TopSite`](../-top-site/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Removes the given [TopSite](../-top-site/index.md). |

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
| [DefaultTopSitesStorage](../-default-top-sites-storage/index.md) | `class DefaultTopSitesStorage : `[`TopSitesStorage`](./index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-observer/index.md)`>`<br>Default implementation of [TopSitesStorage](./index.md). |
