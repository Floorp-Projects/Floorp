[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [SearchesMeasurement](./index.md)

# SearchesMeasurement

`open class SearchesMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/SearchesMeasurement.java#L22)

A TelemetryMeasurement implementation to count the number of times a user has searched with a specific engine from a specific location.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SearchesMeasurement(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.md)`)` |

### Properties

| Name | Summary |
|---|---|
| [LOCATION_ACTIONBAR](-l-o-c-a-t-i-o-n_-a-c-t-i-o-n-b-a-r.md) | `static val LOCATION_ACTIONBAR: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [LOCATION_LISTITEM](-l-o-c-a-t-i-o-n_-l-i-s-t-i-t-e-m.md) | `static val LOCATION_LISTITEM: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [LOCATION_SUGGESTION](-l-o-c-a-t-i-o-n_-s-u-g-g-e-s-t-i-o-n.md) | `static val LOCATION_SUGGESTION: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| Name | Summary |
|---|---|
| [flush](flush.md) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |
| [recordSearch](record-search.md) | `open fun recordSearch(location: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Record a search for the given location and search engine identifier. |

### Inherited Functions

| Name | Summary |
|---|---|
| [getFieldName](../-telemetry-measurement/get-field-name.md) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
