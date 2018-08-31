---
title: LocaleMeasurement - 
---

[org.mozilla.telemetry.measurement](../index.html) / [LocaleMeasurement](./index.html)

# LocaleMeasurement

`open class LocaleMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.html)

### Constructors

| [&lt;init&gt;](-init-.html) | `LocaleMeasurement()` |

### Functions

| [flush](flush.html) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |
| [getLanguageTag](get-language-tag.html) | `open fun getLanguageTag(locale: `[`Locale`](http://docs.oracle.com/javase/6/docs/api/java/util/Locale.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Gecko uses locale codes like "es-ES", whereas a Java Locale stringifies as "es_ES". This method approximates the Java 7 method `Locale#toLanguageTag()`. |

### Inherited Functions

| [getFieldName](../-telemetry-measurement/get-field-name.html) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

