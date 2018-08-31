---
title: TelemetryMeasurement - 
---

[org.mozilla.telemetry.measurement](../index.html) / [TelemetryMeasurement](./index.html)

# TelemetryMeasurement

`abstract class TelemetryMeasurement`

### Constructors

| [&lt;init&gt;](-init-.html) | `TelemetryMeasurement(fieldName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)` |

### Functions

| [flush](flush.html) | `abstract fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)<br>Flush this measurement in order for serializing a ping. Calling this method should create an Object representing the current state of this measurement. Optionally this measurement might be reset. For example a TelemetryMeasurement implementation for the OS version of the device might just return a String like "7.0.1". However a TelemetryMeasurement implementation for counting the usage of search engines might return a HashMap mapping search engine names to search counts. Additionally those counts will be reset after flushing. |
| [getFieldName](get-field-name.html) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| [ArchMeasurement](../-arch-measurement/index.html) | `open class ArchMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [ClientIdMeasurement](../-client-id-measurement/index.html) | `open class ClientIdMeasurement : `[`TelemetryMeasurement`](./index.md)<br>A unique, randomly generated UUID for this client. |
| [CreatedDateMeasurement](../-created-date-measurement/index.html) | `open class CreatedDateMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [CreatedDateMeasurementNew](../-created-date-measurement-new/index.html) | `open class CreatedDateMeasurementNew : `[`TelemetryMeasurement`](./index.md)<br>The field 'created' from CreatedDateMeasurement will be deprecated for the `createdDate` field |
| [CreatedTimestampMeasurement](../-created-timestamp-measurement/index.html) | `open class CreatedTimestampMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [CreatedTimestampMeasurementNew](../-created-timestamp-measurement-new/index.html) | `open class CreatedTimestampMeasurementNew : `[`TelemetryMeasurement`](./index.md)<br>The field 'created' from CreatedTimestampMeasurement will be deprecated for the `createdTimestamp` field |
| [DefaultSearchMeasurement](../-default-search-measurement/index.html) | `open class DefaultSearchMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [DeviceMeasurement](../-device-measurement/index.html) | `open class DeviceMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [EventsMeasurement](../-events-measurement/index.html) | `open class EventsMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [ExperimentsMeasurement](../-experiments-measurement/index.html) | `open class ExperimentsMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [FirstRunProfileDateMeasurement](../-first-run-profile-date-measurement/index.html) | `open class FirstRunProfileDateMeasurement : `[`TelemetryMeasurement`](./index.md)<br>This measurement will save the timestamp of the first time it was instantiated and report this as profile creation date. |
| [LocaleMeasurement](../-locale-measurement/index.html) | `open class LocaleMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [MetricsMeasurement](../-metrics-measurement/index.html) | `open class MetricsMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [ProcessStartTimestampMeasurement](../-process-start-timestamp-measurement/index.html) | `open class ProcessStartTimestampMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [SearchesMeasurement](../-searches-measurement/index.html) | `open class SearchesMeasurement : `[`TelemetryMeasurement`](./index.md)<br>A TelemetryMeasurement implementation to count the number of times a user has searched with a specific engine from a specific location. |
| [SequenceMeasurement](../-sequence-measurement/index.html) | `open class SequenceMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [SessionCountMeasurement](../-session-count-measurement/index.html) | `open class SessionCountMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [SessionDurationMeasurement](../-session-duration-measurement/index.html) | `open class SessionDurationMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [SettingsMeasurement](../-settings-measurement/index.html) | `open class SettingsMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [StaticMeasurement](../-static-measurement/index.html) | `open class StaticMeasurement : `[`TelemetryMeasurement`](./index.md) |
| [TimezoneOffsetMeasurement](../-timezone-offset-measurement/index.html) | `open class TimezoneOffsetMeasurement : `[`TelemetryMeasurement`](./index.md) |

