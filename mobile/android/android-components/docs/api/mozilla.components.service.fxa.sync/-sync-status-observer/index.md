[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [SyncStatusObserver](./index.md)

# SyncStatusObserver

`interface SyncStatusObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L51)

An interface for consumers that wish to observer "sync lifecycle" events.

### Functions

| Name | Summary |
|---|---|
| [onError](on-error.md) | `abstract fun onError(error: `[`Exception`](http://docs.oracle.com/javase/7/docs/api/java/lang/Exception.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called if sync encounters an error that's worthy of processing by status observers. |
| [onIdle](on-idle.md) | `abstract fun onIdle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called at the end of a sync, after every configured syncable has been synchronized. A set of enabled [SyncEngine](../../mozilla.components.service.fxa/-sync-engine/index.md)s could have changed, so observers are expected to query [SyncEnginesStorage.getStatus](../../mozilla.components.service.fxa.manager/-sync-engines-storage/get-status.md). |
| [onStarted](on-started.md) | `abstract fun onStarted(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Gets called at the start of a sync, before any configured syncable is synchronized. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
