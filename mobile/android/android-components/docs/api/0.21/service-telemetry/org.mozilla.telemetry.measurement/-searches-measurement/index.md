---
title: SearchesMeasurement - 
---

[org.mozilla.telemetry.measurement](../index.html) / [SearchesMeasurement](./index.html)

# SearchesMeasurement

`open class SearchesMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.html)

A TelemetryMeasurement implementation to count the number of times a user has searched with a specific engine from a specific location.

### Constructors

| [&lt;init&gt;](-init-.html) | `SearchesMeasurement(configuration: `[`TelemetryConfiguration`](../../org.mozilla.telemetry.config/-telemetry-configuration/index.html)`)` |

### Properties

| [LOCATION_ACTIONBAR](-l-o-c-a-t-i-o-n_-a-c-t-i-o-n-b-a-r.html) | `static val LOCATION_ACTIONBAR: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [LOCATION_LISTITEM](-l-o-c-a-t-i-o-n_-l-i-s-t-i-t-e-m.html) | `static val LOCATION_LISTITEM: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [LOCATION_SUGGESTION](-l-o-c-a-t-i-o-n_-s-u-g-g-e-s-t-i-o-n.html) | `static val LOCATION_SUGGESTION: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Functions

| [flush](flush.html) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |
| [recordSearch](record-search.html) | `open fun recordSearch(location: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, identifier: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Record a search for the given location and search engine identifier. |

### Inherited Functions

| [getFieldName](../-telemetry-measurement/get-field-name.html) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

