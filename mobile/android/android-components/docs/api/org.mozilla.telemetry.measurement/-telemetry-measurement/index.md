[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [TelemetryMeasurement](./index.md)

# TelemetryMeasurement

`abstract class TelemetryMeasurement` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/TelemetryMeasurement.java#L7)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TelemetryMeasurement(fieldName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)` |

### Functions

| Name | Summary |
|---|---|
| [flush](flush.md) | `abstract fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)<br>Flush this measurement in order for serializing a ping. Calling this method should create an Object representing the current state of this measurement. Optionally this measurement might be reset. For example a TelemetryMeasurement implementation for the OS version of the device might just return a String like "7.0.1". However a TelemetryMeasurement implementation for counting the usage of search engines might return a HashMap mapping search engine names to search counts. Additionally those counts will be reset after flushing. |
| [getFieldName](get-field-name.md) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [ArchMeasurement](../-arch-measurement/index.md) | `open class ArchMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [ClientIdMeasurement](../-client-id-measurement/index.md) | `open class ClientIdMeasurement : `[`TelemetryMeasurement`](./index.md)<br>A unique, randomly generated UUID for this client. |
| [CreatedDateMeasurement](../-created-date-measurement/index.md) | `open class CreatedDateMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [CreatedDateMeasurementNew](../-created-date-measurement-new/index.md) | `open class CreatedDateMeasurementNew : `[`TelemetryMeasurement`](./index.md)<br>The field 'created' from CreatedDateMeasurement will be deprecated for the `createdDate` field |
| [CreatedTimestampMeasurement](../-created-timestamp-measurement/index.md) | `open class CreatedTimestampMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [CreatedTimestampMeasurementNew](../-created-timestamp-measurement-new/index.md) | `open class CreatedTimestampMeasurementNew : `[`TelemetryMeasurement`](./index.md)<br>The field 'created' from CreatedTimestampMeasurement will be deprecated for the `createdTimestamp` field |
| [DefaultSearchMeasurement](../-default-search-measurement/index.md) | `open class DefaultSearchMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [DeviceMeasurement](../-device-measurement/index.md) | `open class DeviceMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [EventsMeasurement](../-events-measurement/index.md) | `open class EventsMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [ExperimentsMapMeasurement](../-experiments-map-measurement/index.md) | `class ExperimentsMapMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [ExperimentsMeasurement](../-experiments-measurement/index.md) | `open class ExperimentsMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [FirstRunProfileDateMeasurement](../-first-run-profile-date-measurement/index.md) | `open class FirstRunProfileDateMeasurement : `[`TelemetryMeasurement`](./index.md)<br>This measurement will save the timestamp of the first time it was instantiated and report this as profile creation date. |
| [LocaleMeasurement](../-locale-measurement/index.md) | `open class LocaleMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [MetricsMeasurement](../-metrics-measurement/index.md) | `open class MetricsMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [PocketIdMeasurement](../-pocket-id-measurement/index.md) | `open class PocketIdMeasurement : `[`TelemetryMeasurement`](./index.md)<br>A unique, randomly generated UUID for this pocket client for fire-tv instance. This is distinct from the telemetry clientId. The clientId should not be able to be tied to the pocketId in any way. |
| [ProcessStartTimestampMeasurement](../-process-start-timestamp-measurement/index.md) | `open class ProcessStartTimestampMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [SearchesMeasurement](../-searches-measurement/index.md) | `open class SearchesMeasurement : `[`TelemetryMeasurement`](./index.md)<br>A TelemetryMeasurement implementation to count the number of times a user has searched with a specific engine from a specific location. |
| [SequenceMeasurement](../-sequence-measurement/index.md) | `open class SequenceMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [SessionCountMeasurement](../-session-count-measurement/index.md) | `open class SessionCountMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [SessionDurationMeasurement](../-session-duration-measurement/index.md) | `open class SessionDurationMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [SettingsMeasurement](../-settings-measurement/index.md) | `open class SettingsMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [StaticMeasurement](../-static-measurement/index.md) | `open class StaticMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [TimezoneOffsetMeasurement](../-timezone-offset-measurement/index.md) | `open class TimezoneOffsetMeasurement : `[`TelemetryMeasurement`](./index.md) |
