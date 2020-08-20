[android-components](../../index.md) / [mozilla.components.support.sync.telemetry](../index.md) / [SyncTelemetry](index.md) / [processBookmarksPing](./process-bookmarks-ping.md)

# processBookmarksPing

`fun processBookmarksPing(ping: SyncTelemetryPing, sendPing: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = { Pings.bookmarksSync.submit() }): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/sync-telemetry/src/main/java/mozilla/components/support/sync/telemetry/SyncTelemetry.kt#L152)

Processes a bookmarks-related ping information from the [ping](process-bookmarks-ping.md#mozilla.components.support.sync.telemetry.SyncTelemetry$processBookmarksPing(mozilla.appservices.sync15.SyncTelemetryPing, kotlin.Function0((kotlin.Unit)))/ping).

**Return**
'false' if global error was encountered, 'true' otherwise.

