---
title: Telemetry - 
---

[org.mozilla.telemetry](../index.html) / [Telemetry](./index.html)

# Telemetry

`open class Telemetry`

### Constructors

| [&lt;init&gt;](-init-.html) | `Telemetry(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`, storage: `[`TelemetryStorage`](../../org.mozilla.telemetry.storage/-telemetry-storage/index.html)`, client: `[`TelemetryClient`](../../org.mozilla.telemetry.net/-telemetry-client/index.html)`, scheduler: `[`TelemetryScheduler`](../../org.mozilla.telemetry.schedule/-telemetry-scheduler/index.html)`)` |

### Functions

| [addPingBuilder](add-ping-builder.html) | `open fun addPingBuilder(builder: `[`TelemetryPingBuilder`](../../org.mozilla.telemetry.ping/-telemetry-ping-builder/index.html)`): `[`Telemetry`](./index.md) |
| [getBuilders](get-builders.html) | `open fun getBuilders(): `[`MutableCollection`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-collection/index.html)`<`[`TelemetryPingBuilder`](../../org.mozilla.telemetry.ping/-telemetry-ping-builder/index.html)`>` |
| [getClient](get-client.html) | `open fun getClient(): `[`TelemetryClient`](../../org.mozilla.telemetry.net/-telemetry-client/index.html) |
| [getConfiguration](get-configuration.html) | `open fun getConfiguration(): `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html) |
| [getStorage](get-storage.html) | `open fun getStorage(): `[`TelemetryStorage`](../../org.mozilla.telemetry.storage/-telemetry-storage/index.html) |
| [queueEvent](queue-event.html) | `open fun queueEvent(event: `[`TelemetryEvent`](../../org.mozilla.telemetry.event/-telemetry-event/index.html)`): `[`Telemetry`](./index.md) |
| [queuePing](queue-ping.html) | `open fun queuePing(pingType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Telemetry`](./index.md) |
| [recordActiveExperiments](record-active-experiments.html) | `open fun recordActiveExperiments(activeExperimentsIds: `[`MutableList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-mutable-list/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>): `[`Telemetry`](./index.md)<br>Records the list of active experiments |
| [recordSearch](record-search.html) | `open fun recordSearch(location: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Telemetry`](./index.md)<br>Record a search for the given location and search engine identifier. Common location values used by Fennec and Focus: actionbar: the user types in the url bar and hits enter to use the default search engine listitem: the user selects a search engine from the list of secondary search engines at the bottom of the screen suggestion: the user clicks on a search suggestion or, in the case that suggestions are disabled, the row corresponding with the main engine |
| [recordSessionEnd](record-session-end.html) | `open fun recordSessionEnd(): `[`Telemetry`](./index.md) |
| [recordSessionStart](record-session-start.html) | `open fun recordSessionStart(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [scheduleUpload](schedule-upload.html) | `open fun scheduleUpload(): `[`Telemetry`](./index.md) |
| [setDefaultSearchProvider](set-default-search-provider.html) | `open fun setDefaultSearchProvider(provider: `[`DefaultSearchEngineProvider`](../../org.mozilla.telemetry.measurement/-default-search-measurement/-default-search-engine-provider/index.html)`): `[`Telemetry`](./index.md) |

