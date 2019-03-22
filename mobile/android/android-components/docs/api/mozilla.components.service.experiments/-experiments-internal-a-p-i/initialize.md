[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [ExperimentsInternalAPI](index.md) / [initialize](./initialize.md)

# initialize

`fun initialize(applicationContext: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, fetchClient: `[`Client`](../../mozilla.components.concept.fetch/-client/index.md)` = HttpURLConnectionClient()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L50)

Initialize the experiments library.

This should only be initialized once by the application.

### Parameters

`applicationContext` - [Context](https://developer.android.com/reference/android/content/Context.html) to access application features, such
as shared preferences.