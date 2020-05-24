[android-components](../../index.md) / [org.mozilla.telemetry.measurement](../index.md) / [LocaleMeasurement](index.md) / [getLanguageTag](./get-language-tag.md)

# getLanguageTag

`open fun getLanguageTag(locale: `[`Locale`](http://docs.oracle.com/javase/7/docs/api/java/util/Locale.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/measurement/LocaleMeasurement.java#L30)

Gecko uses locale codes like "es-ES", whereas a Java Locale stringifies as "es_ES". This method approximates the Java 7 method `Locale#toLanguageTag()`.

**Return**
a locale string suitable for passing to Gecko.

