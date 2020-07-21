[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [StaticMeasurement](./index.md)

# StaticMeasurement

`open class StaticMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/StaticMeasurement.java#L7)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `StaticMeasurement(fieldName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`)` |

### Functions

| Name | Summary |
|---|---|
| [flush](flush.md) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [getFieldName](../-telemetry-measurement/get-field-name.md) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [OperatingSystemMeasurement](../-operating-system-measurement/index.md) | `open class OperatingSystemMeasurement : `[`StaticMeasurement`](./index.md) |
| [OperatingSystemVersionMeasurement](../-operating-system-version-measurement/index.md) | `open class OperatingSystemVersionMeasurement : `[`StaticMeasurement`](./index.md) |
| [VersionMeasurement](../-version-measurement/index.md) | `open class VersionMeasurement : `[`StaticMeasurement`](./index.md) |
