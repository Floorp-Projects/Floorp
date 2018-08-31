---
title: LocaleMeasurement.getLanguageTag - 
---

[org.mozilla.telemetry.measurement](../index.html) / [LocaleMeasurement](index.html) / [getLanguageTag](./get-language-tag.html)

# getLanguageTag

`open fun getLanguageTag(locale: `[`Locale`](http://docs.oracle.com/javase/6/docs/api/java/util/Locale.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)

Gecko uses locale codes like "es-ES", whereas a Java Locale stringifies as "es_ES". This method approximates the Java 7 method `Locale#toLanguageTag()`.

**Return**
a locale string suitable for passing to Gecko.

