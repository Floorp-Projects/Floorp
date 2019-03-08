[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [Experiments](index.md) / [setOverrideNow](./set-override-now.md)

# setOverrideNow

`fun setOverrideNow(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.md)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L143)

Overrides a specified experiment as a blocking operation

### Exceptions

`IllegalArgumentException` - when called from the main thread

### Parameters

`context` - context

`descriptor` - descriptor of the experiment

`active` - overridden value for the experiment, true to activate it, false to deactivate