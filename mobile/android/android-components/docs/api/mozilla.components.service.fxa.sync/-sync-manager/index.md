[android-components](../../index.md) / [mozilla.components.service.fxa.sync](../index.md) / [SyncManager](./index.md)

# SyncManager

`abstract class SyncManager` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/sync/SyncManager.kt#L110)

A base sync manager implementation.

### Parameters

`syncConfig` - A [SyncConfig](../../mozilla.components.service.fxa/-sync-config/index.md) object describing how sync should behave.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SyncManager(syncConfig: `[`SyncConfig`](../../mozilla.components.service.fxa/-sync-config/index.md)`)`<br>A base sync manager implementation. |

### Properties

| Name | Summary |
|---|---|
| [logger](logger.md) | `open val logger: `[`Logger`](../../mozilla.components.support.base.log.logger/-logger/index.md) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [SYNC_PERIOD_UNIT](-s-y-n-c_-p-e-r-i-o-d_-u-n-i-t.md) | `val SYNC_PERIOD_UNIT: `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
