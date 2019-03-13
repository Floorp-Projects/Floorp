[android-components](../../index.md) / [mozilla.components.support.base.observer](../index.md) / [Observable](./index.md)

# Observable

`interface Observable<T>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/observer/Observable.kt#L18)

Interface for observables. This interface is implemented by ObserverRegistry so that classes that
want to be observable can implement the interface by delegation:

```
    class MyObservableClass : Observable<MyObserverInterface> by registry {
      ...
    }
```

### Functions

| Name | Summary |
|---|---|
| [notifyObservers](notify-observers.md) | `abstract fun notifyObservers(block: `[`T`](index.md#T)`.() -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Notifies all registered observers about a change. |
| [pauseObserver](pause-observer.md) | `abstract fun pauseObserver(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Pauses the provided observer. No notifications will be sent to this observer until [resumeObserver](resume-observer.md) is called. |
| [register](register.md) | `abstract fun register(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](index.md#T)`, owner: LifecycleOwner, autoPause: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun register(observer: `[`T`](index.md#T)`, view: `[`View`](https://developer.android.com/reference/android/view/View.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers an observer to get notified about changes. |
| [resumeObserver](resume-observer.md) | `abstract fun resumeObserver(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resumes the provided observer. Notifications sent since it was last paused (see [pauseObserver](pause-observer.md)]) are lost and will not be re-delivered. |
| [unregister](unregister.md) | `abstract fun unregister(observer: `[`T`](index.md#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters an observer. |
| [unregisterObservers](unregister-observers.md) | `abstract fun unregisterObservers(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters all observers. |
| [wrapConsumers](wrap-consumers.md) | `abstract fun <R> wrapConsumers(block: `[`T`](index.md#T)`.(`[`R`](wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<(`[`R`](wrap-consumers.md#R)`) -> `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>`<br>Returns a list of lambdas wrapping a consuming method of an observer. |

### Inheritors

| Name | Summary |
|---|---|
| [EngineSession](../../mozilla.components.concept.engine/-engine-session/index.md) | `abstract class EngineSession : `[`Observable`](./index.md)`<`[`Observer`](../../mozilla.components.concept.engine/-engine-session/-observer/index.md)`>`<br>Class representing a single engine session. |
| [FxaAccountManager](../../mozilla.components.service.fxa/-fxa-account-manager/index.md) | `open class FxaAccountManager : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](./index.md)`<`[`AccountObserver`](../../mozilla.components.concept.sync/-account-observer/index.md)`>`<br>An account manager which encapsulates various internal details of an account lifecycle and provides an observer interface along with a public API for interacting with an account. The internal state machine abstracts over state space as exposed by the fxaclient library, not the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us. |
| [GeneralSyncManager](../../mozilla.components.feature.sync/-general-sync-manager/index.md) | `abstract class GeneralSyncManager : `[`SyncManager`](../../mozilla.components.concept.sync/-sync-manager/index.md)`, `[`Observable`](./index.md)`<`[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)`>, `[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)<br>A base SyncManager implementation which manages a dispatcher, handles authentication and requests synchronization in the following manner: On authentication AND on store set changes (add or remove)... |
| [ObserverRegistry](../-observer-registry/index.md) | `class ObserverRegistry<T> : `[`Observable`](./index.md)`<`[`T`](../-observer-registry/index.md#T)`>`<br>A helper for classes that want to get observed. This class keeps track of registered observers and can automatically unregister observers if a LifecycleOwner is provided. |
| [Session](../../mozilla.components.browser.session/-session/index.md) | `class Session : `[`Observable`](./index.md)`<`[`Observer`](../../mozilla.components.browser.session/-session/-observer/index.md)`>`<br>Value type that represents the state of a browser session. Changes can be observed. |
| [SessionManager](../../mozilla.components.browser.session/-session-manager/index.md) | `class SessionManager : `[`Observable`](./index.md)`<`[`Observer`](../../mozilla.components.browser.session/-session-manager/-observer/index.md)`>`<br>This class provides access to a centralized registry of all active sessions. |
| [StorageSync](../../mozilla.components.feature.sync/-storage-sync/index.md) | `class StorageSync : `[`Observable`](./index.md)`<`[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)`>`<br>A feature implementation which orchestrates data synchronization of a set of [SyncableStore](../../mozilla.components.concept.sync/-syncable-store/index.md)-s. |
| [SyncDispatcher](../../mozilla.components.feature.sync/-sync-dispatcher/index.md) | `interface SyncDispatcher : `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html)`, `[`Observable`](./index.md)`<`[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)`>`<br>Internal interface to enable testing SyncManager implementations independently from SyncDispatcher. |
| [SyncManager](../../mozilla.components.concept.sync/-sync-manager/index.md) | `interface SyncManager : `[`Observable`](./index.md)`<`[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)`>`<br>Describes a "sync" entry point for an application. |
| [TabsAdapter](../../mozilla.components.browser.tabstray/-tabs-adapter/index.md) | `class TabsAdapter : Adapter<`[`TabViewHolder`](../../mozilla.components.browser.tabstray/-tab-view-holder/index.md)`>, `[`TabsTray`](../../mozilla.components.concept.tabstray/-tabs-tray/index.md)`, `[`Observable`](./index.md)`<`[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`>`<br>RecyclerView adapter implementation to display a list/grid of tabs. |
| [TabsTray](../../mozilla.components.concept.tabstray/-tabs-tray/index.md) | `interface TabsTray : `[`Observable`](./index.md)`<`[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`>`<br>Generic interface for components that provide "tabs tray" functionality. |
| [WorkManagerSyncDispatcher](../../mozilla.components.feature.sync/-work-manager-sync-dispatcher/index.md) | `class WorkManagerSyncDispatcher : `[`SyncDispatcher`](../../mozilla.components.feature.sync/-sync-dispatcher/index.md)`, `[`Observable`](./index.md)`<`[`SyncStatusObserver`](../../mozilla.components.concept.sync/-sync-status-observer/index.md)`>, `[`Closeable`](https://developer.android.com/reference/java/io/Closeable.html) |
