[android-components](../../index.md) / [mozilla.components.support.sync.telemetry](../index.md) / [SyncTelemetry](./index.md)

# SyncTelemetry

`object SyncTelemetry` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/sync-telemetry/src/main/java/mozilla/components/support/sync/telemetry/SyncTelemetry.kt#L21)

Contains functionality necessary to process instances of [SyncTelemetryPing](#).

### Functions

| Name | Summary |
|---|---|
| [processBookmarksPing](process-bookmarks-ping.md) | `fun processBookmarksPing(ping: SyncTelemetryPing, sendPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.bookmarksSync.send() }): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes a bookmarks-related ping information from the [ping](process-bookmarks-ping.md#mozilla.components.support.sync.telemetry.SyncTelemetry$processBookmarksPing(mozilla.appservices.sync15.SyncTelemetryPing, kotlin.Function0((kotlin.Unit)))/ping). |
| [processHistoryPing](process-history-ping.md) | `fun processHistoryPing(ping: SyncTelemetryPing, sendPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.historySync.send() }): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes a history-related ping information from the [ping](process-history-ping.md#mozilla.components.support.sync.telemetry.SyncTelemetry$processHistoryPing(mozilla.appservices.sync15.SyncTelemetryPing, kotlin.Function0((kotlin.Unit)))/ping). |
