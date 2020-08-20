[android-components](../../index.md) / [mozilla.components.support.sync.telemetry](../index.md) / [SyncTelemetry](index.md) / [processSyncTelemetry](./process-sync-telemetry.md)

# processSyncTelemetry

`fun processSyncTelemetry(syncTelemetry: SyncTelemetryPing, submitGlobalPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.sync.submit() }, submitHistoryPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.historySync.submit() }, submitBookmarksPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.bookmarksSync.submit() }, submitLoginsPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.loginsSync.submit() }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/sync-telemetry/src/main/java/mozilla/components/support/sync/telemetry/SyncTelemetry.kt#L44)

Process [SyncTelemetryPing](#) as returned from [mozilla.appservices.syncmanager.SyncManager](#).

