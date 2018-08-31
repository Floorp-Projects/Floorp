---
title: TelemetryEvent.create - 
---

[org.mozilla.telemetry.event](../index.html) / [TelemetryEvent](index.html) / [create](./create.html)

# create

`@CheckResult open static fun create(@NonNull category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @NonNull method: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @Nullable object: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`TelemetryEvent`](index.html)

Create a new event with mandatory category, method and object.

### Parameters

`category` - identifier. The category is a group name for events and helps to avoid name conflicts.

`method` - identifier. This describes the type of event that occured, e.g. click, keydown or focus.

`object` - identifier. This is the object the event occured on, e.g. reload_button or urlbar.`@CheckResult open static fun create(@NonNull category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @NonNull method: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @Nullable object: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TelemetryEvent`](index.html)

Create a new event with mandatory category, method, object and value.

### Parameters

`category` - identifier. The category is a group name for events and helps to avoid name conflicts.

`method` - identifier. This describes the type of event that occured, e.g. click, keydown or focus.

`object` - identifier. This is the object the event occured on, e.g. reload_button or urlbar.

`value` - This is a user defined value, providing context for the event.