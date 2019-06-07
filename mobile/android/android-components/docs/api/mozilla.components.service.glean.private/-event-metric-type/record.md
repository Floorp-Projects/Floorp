[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [EventMetricType](index.md) / [record](./record.md)

# record

`fun record(extra: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`ExtraKeysEnum`](index.md#ExtraKeysEnum)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/EventMetricType.kt#L50)

Record an event by using the information provided by the instance of this class.

### Parameters

`extra` - optional. This is map, both keys and values need to be strings, keys are
    identifiers. This is used for events where additional richer context is needed.
    The maximum length for values is defined by [MAX_LENGTH_EXTRA_KEY_VALUE](#)