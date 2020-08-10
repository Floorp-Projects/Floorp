[android-components](../../index.md) / [mozilla.components.support.sync.telemetry](../index.md) / [SyncTelemetry](./index.md)

# SyncTelemetry

`object SyncTelemetry` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/sync-telemetry/src/main/java/mozilla/components/support/sync/telemetry/SyncTelemetry.kt#L26)

Contains functionality necessary to process instances of [SyncTelemetryPing](#).

### Functions

| Name | Summary |
|---|---|
| [processBookmarksPing](process-bookmarks-ping.md) | `fun processBookmarksPing(ping: SyncTelemetryPing, sendPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.bookmarksSync.submit() }): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes a bookmarks-related ping information from the [ping](process-bookmarks-ping.md#mozilla.components.support.sync.telemetry.SyncTelemetry$processBookmarksPing(mozilla.appservices.sync15.SyncTelemetryPing, kotlin.Function0((kotlin.Unit)))/ping). |
| [processHistoryPing](process-history-ping.md) | `fun processHistoryPing(ping: SyncTelemetryPing, sendPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.historySync.submit() }): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes a history-related ping information from the [ping](process-history-ping.md#mozilla.components.support.sync.telemetry.SyncTelemetry$processHistoryPing(mozilla.appservices.sync15.SyncTelemetryPing, kotlin.Function0((kotlin.Unit)))/ping). |
| [processLoginsPing](process-logins-ping.md) | `fun processLoginsPing(ping: SyncTelemetryPing, sendPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.loginsSync.submit() }): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Processes a passwords-related ping information from the [ping](process-logins-ping.md#mozilla.components.support.sync.telemetry.SyncTelemetry$processLoginsPing(mozilla.appservices.sync15.SyncTelemetryPing, kotlin.Function0((kotlin.Unit)))/ping). |
| [processSyncTelemetry](process-sync-telemetry.md) | `fun processSyncTelemetry(syncTelemetry: SyncTelemetryPing, submitGlobalPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.sync.submit() }, submitHistoryPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.historySync.submit() }, submitBookmarksPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.bookmarksSync.submit() }, submitLoginsPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.loginsSync.submit() }): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Process [SyncTelemetryPing](#) as returned from [mozilla.appservices.syncmanager.SyncManager](#). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
