[android-components](../../index.md) / [mozilla.components.support.sync.telemetry](../index.md) / [SyncTelemetry](index.md) / [processHistoryPing](./process-history-ping.md)

# processHistoryPing

`fun processHistoryPing(ping: SyncTelemetryPing, sendPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.historySync.submit() }): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/sync-telemetry/src/main/java/mozilla/components/support/sync/telemetry/SyncTelemetry.kt#L88)

Processes a history-related ping information from the [ping](process-history-ping.md#mozilla.components.support.sync.telemetry.SyncTelemetry$processHistoryPing(mozilla.appservices.sync15.SyncTelemetryPing, kotlin.Function0((kotlin.Unit)))/ping).

**Return**
'false' if global error was encountered, 'true' otherwise.

