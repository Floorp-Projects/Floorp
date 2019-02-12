[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [EventMetricType](index.md) / [record](./record.md)

# record

`fun record(objectId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, value: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, extra: `[`Map`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-map/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/EventMetricType.kt#L55)

Record an event by using the information provided by the instance of this class.

### Parameters

`objectId` - the object the event occurred on, e.g. 'reload_button'. The maximum
    length of this string is defined by [MAX_LENGTH_OBJECT_ID](#)

`value` - optional. This is a user defined value, providing context for the event. The
    maximum length of this string is defined by [MAX_LENGTH_VALUE](#)

`extra` - optional. This is map, both keys and values need to be strings, keys are
    identifiers. This is used for events where additional richer context is needed.
    The maximum length for values is defined by [MAX_LENGTH_EXTRA_KEY_VALUE](#)