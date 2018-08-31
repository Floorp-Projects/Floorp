---
title: TelemetryMeasurement.flush - 
---

[org.mozilla.telemetry.measurement](../index.html) / [TelemetryMeasurement](index.html) / [flush](./flush.html)

# flush

`abstract fun flush(): `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)

Flush this measurement in order for serializing a ping. Calling this method should create an Object representing the current state of this measurement. Optionally this measurement might be reset. For example a TelemetryMeasurement implementation for the OS version of the device might just return a String like "7.0.1". However a TelemetryMeasurement implementation for counting the usage of search engines might return a HashMap mapping search engine names to search counts. Additionally those counts will be reset after flushing.

