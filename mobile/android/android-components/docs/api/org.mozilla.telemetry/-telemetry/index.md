[android-components](../../index.md) / [org.mozilla.telemetry](../index.md) / [Telemetry](./index.md)

# Telemetry

`open class Telemetry` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/Telemetry.java#L43)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Telemetry(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`, storage: `[`TelemetryStorage`](../../org.mozilla.telemetry.storage/-telemetry-storage/index.md)`, client: `[`TelemetryClient`](../../org.mozilla.telemetry.net/-telemetry-client/index.md)`, scheduler: `[`TelemetryScheduler`](../../org.mozilla.telemetry.schedule/-telemetry-scheduler/index.md)`)` |

### Functions

| Name | Summary |
|---|---|
| [addPingBuilder](add-ping-builder.md) | `open fun addPingBuilder(builder: `[`TelemetryPingBuilder`](../../org.mozilla.telemetry.ping/-telemetry-ping-builder/index.md)`): `[`Telemetry`](./index.md) |
| [getBuilders](get-builders.md) | `open fun getBuilders(): `[`MutableCollection`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-collection/index.html)`<`[`TelemetryPingBuilder`](../../org.mozilla.telemetry.ping/-telemetry-ping-builder/index.md)`>` |
| [getClient](get-client.md) | `open fun getClient(): `[`TelemetryClient`](../../org.mozilla.telemetry.net/-telemetry-client/index.md) |
| [getClientId](get-client-id.md) | `open fun getClientId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns the unique client id for this installation (UUID). |
| [getConfiguration](get-configuration.md) | `open fun getConfiguration(): `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md) |
| [getPingBuilder](get-ping-builder.md) | `open fun getPingBuilder(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TelemetryPingBuilder`](../../org.mozilla.telemetry.ping/-telemetry-ping-builder/index.md)<br>Returns a previously added ping builder or null if no ping builder of the given type has been added. |
| [getStorage](get-storage.md) | `open fun getStorage(): `[`TelemetryStorage`](../../org.mozilla.telemetry.storage/-telemetry-storage/index.md) |
| [queueEvent](queue-event.md) | `open fun queueEvent(event: `[`TelemetryEvent`](../../org.mozilla.telemetry.event/-telemetry-event/index.md)`): `[`Telemetry`](./index.md) |
| [queuePing](queue-ping.md) | `open fun queuePing(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Telemetry`](./index.md) |
| [recordActiveExperiments](record-active-experiments.md) | `open fun recordActiveExperiments(activeExperimentsIds: `[`MutableList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Telemetry`](./index.md)<br>Records the list of active experiments |
| [recordExperiments](record-experiments.md) | `open fun recordExperiments(experiments: `[`MutableMap`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`>): `[`Telemetry`](./index.md)<br>Records all experiments the client knows of in the event ping. |
| [recordSearch](record-search.md) | `open fun recordSearch(location: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Telemetry`](./index.md)<br>Record a search for the given location and search engine identifier. Common location values used by Fennec and Focus: actionbar: the user types in the url bar and hits enter to use the default search engine listitem: the user selects a search engine from the list of secondary search engines at the bottom of the screen suggestion: the user clicks on a search suggestion or, in the case that suggestions are disabled, the row corresponding with the main engine |
| [recordSessionEnd](record-session-end.md) | `open fun recordSessionEnd(onFailure: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Telemetry`](./index.md)<br>`open fun recordSessionEnd(): `[`Telemetry`](./index.md) |
| [recordSessionStart](record-session-start.md) | `open fun recordSessionStart(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [scheduleUpload](schedule-upload.md) | `open fun scheduleUpload(): `[`Telemetry`](./index.md) |
| [setDefaultSearchProvider](set-default-search-provider.md) | `open fun setDefaultSearchProvider(provider: `[`DefaultSearchEngineProvider`](../../org.mozilla.telemetry.measurement/-default-search-measurement/-default-search-engine-provider/index.md)`): `[`Telemetry`](./index.md) |
