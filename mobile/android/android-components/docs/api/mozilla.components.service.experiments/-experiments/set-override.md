[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [Experiments](index.md) / [setOverride](./set-override.md)

# setOverride

`fun setOverride(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, descriptor: `[`ExperimentDescriptor`](../-experiment-descriptor/index.md)`, active: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L131)

Overrides a specified experiment asynchronously

### Parameters

`context` - context

`descriptor` - descriptor of the experiment

`active` - overridden value for the experiment, true to activate it, false to deactivate