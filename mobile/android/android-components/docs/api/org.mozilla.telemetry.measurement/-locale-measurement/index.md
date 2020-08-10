[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [LocaleMeasurement](./index.md)

# LocaleMeasurement

`open class LocaleMeasurement : `[`TelemetryMeasurement`](../-telemetry-measurement/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/LocaleMeasurement.java#L9)

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LocaleMeasurement()` |

### Functions

| Name | Summary |
|---|---|
| [flush](flush.md) | `open fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html) |
| [getLanguageTag](get-language-tag.md) | `open fun getLanguageTag(locale: `[`Locale`](http://docs.oracle.com/javase/7/docs/api/java/util/Locale.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Gecko uses locale codes like "es-ES", whereas a Java Locale stringifies as "es_ES". This method approximates the Java 7 method `Locale#toLanguageTag()`. |

### Inherited Functions

| Name | Summary |
|---|---|
| [getFieldName](../-telemetry-measurement/get-field-name.md) | `open fun getFieldName(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
