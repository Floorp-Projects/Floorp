[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [ExperimentsInternalAPI](index.md) / [initialize](./initialize.md)

# initialize

`fun initialize(applicationContext: <ERROR CLASS>, configuration: `[`Configuration`](../-configuration/index.md)` = Configuration()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/Experiments.kt#L57)

Initialize the experiments library.

This should only be initialized once by the application.

### Parameters

`applicationContext` - [Context](#) to access application features, such
as shared preferences.  As we cannot enforce through the compiler that the context pass to
the initialize function is a applicationContext, there could potentially be a memory leak
if the initializing application doesn't comply.

`configuration` - [Configuration](../-configuration/index.md) containing information about the experiments endpoint.