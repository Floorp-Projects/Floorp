[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [Experiments](index.md) / [clearOverrideNow](./clear-override-now.md)

# clearOverrideNow

`fun clearOverrideNow(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L165)

Clears an override for a specified experiment as a blocking operation

### Exceptions

`IllegalArgumentException` - when called from the main thread

### Parameters

`context` - context

`descriptor` - descriptor of the experiment