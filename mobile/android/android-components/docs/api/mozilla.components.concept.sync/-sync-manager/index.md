[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncManager](./index.md)

# SyncManager

`interface SyncManager : `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../-sync-status-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/Sync.kt#L41)

Describes a "sync" entry point for an application.

### Functions

| Name | Summary |
|---|---|
| [addStore](add-store.md) | `abstract fun addStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a store, by [name](add-store.md#mozilla.components.concept.sync.SyncManager$addStore(kotlin.String)/name), to a set of synchronization stores. Implementation is expected to be able to either access or instantiate concrete [SyncableStore](../-syncable-store/index.md) instances given this name. |
| [authenticated](authenticated.md) | `abstract fun authenticated(account: `[`OAuthAccount`](../-o-auth-account/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>An authenticated account is now available. |
| [isSyncRunning](is-sync-running.md) | `abstract fun isSyncRunning(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicates if synchronization is currently active. |
| [loggedOut](logged-out.md) | `abstract fun loggedOut(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Previously authenticated account has been logged-out. |
| [removeStore](remove-store.md) | `abstract fun removeStore(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Remove a store previously specified via [addStore](add-store.md) from a set of synchronization stores. |
| [syncNow](sync-now.md) | `abstract fun syncNow(startup: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Request an immediate synchronization of all configured stores. |

### Inherited Functions

| Name | Summary |
|---|---|
| [notifyObservers](../../mozilla.components.support.base.observer/-observable/notify-observers.md) | `abstract fun notifyObservers(block: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers about a change. |
| [pauseObserver](../../mozilla.components.support.base.observer/-observable/pause-observer.md) | `abstract fun pauseObserver(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Pauses the provided observer. No notifications will be sent to this observer until [resumeObserver](../../mozilla.components.support.base.observer/-observable/resume-observer.md) is called. |
| [register](../../mozilla.components.support.base.observer/-observable/register.md) | `abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`, view: `[`View`](https://developer.android.com/reference/android/view/View.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. |
| [resumeObserver](../../mozilla.components.support.base.observer/-observable/resume-observer.md) | `abstract fun resumeObserver(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resumes the provided observer. Notifications sent since it was last paused (see [pauseObserver](../../mozilla.components.support.base.observer/-observable/pause-observer.md)]) are lost and will not be re-delivered. |
| [unregister](../../mozilla.components.support.base.observer/-observable/unregister.md) | `abstract fun unregister(observer: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer. |
| [unregisterObservers](../../mozilla.components.support.base.observer/-observable/unregister-observers.md) | `abstract fun unregisterObservers(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters all observers. |
| [wrapConsumers](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md) | `abstract fun <R> wrapConsumers(block: `[`T`](../../mozilla.components.support.base.observer/-observable/index.md#T)`.(`[`R`](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`R`](../../mozilla.components.support.base.observer/-observable/wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Returns a list of lambdas wrapping a consuming method of an observer. |

### Inheritors

| Name | Summary |
|---|---|
| [GeneralSyncManager](../../mozilla.components.feature.sync/-general-sync-manager/index.md) | `abstract class GeneralSyncManager : `[`SyncManager`](./index.md)`, `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`SyncStatusObserver`](../-sync-status-observer/index.md)`>, `[`SyncStatusObserver`](../-sync-status-observer/index.md)<br>A base SyncManager implementation which manages a dispatcher, handles authentication and requests synchronization in the following manner: On authentication AND on store set changes (add or remove)... |
