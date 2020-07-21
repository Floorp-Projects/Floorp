[android-components](../../index.md) / [org.mozilla.telemetry.event](../index.md) / [TelemetryEvent](index.md) / [create](./create.md)

# create

`@CheckResult open static fun create(@NonNull category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @NonNull method: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @Nullable object: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TelemetryEvent`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/event/TelemetryEvent.java#L43)

Create a new event with mandatory category, method and object.

### Parameters

`category` - identifier. The category is a group name for events and helps to avoid name conflicts.

`method` - identifier. This describes the type of event that occured, e.g. click, keydown or focus.

`object` - identifier. This is the object the event occured on, e.g. reload_button or urlbar.`@CheckResult open static fun create(@NonNull category: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @NonNull method: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, @Nullable object: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`TelemetryEvent`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/telemetry/src/main/java/org/mozilla/telemetry/event/TelemetryEvent.java#L56)

Create a new event with mandatory category, method, object and value.

### Parameters

`category` - identifier. The category is a group name for events and helps to avoid name conflicts.

`method` - identifier. This describes the type of event that occured, e.g. click, keydown or focus.

`object` - identifier. This is the object the event occured on, e.g. reload_button or urlbar.

`value` - This is a user defined value, providing context for the event.