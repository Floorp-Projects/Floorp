[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [GleanInternalAPI](index.md) / [initialize](./initialize.md)

# initialize

`fun initialize(applicationContext: <ERROR CLASS>, configuration: `[`Configuration`](../../mozilla.components.service.glean.config/-configuration/index.md)` = Configuration()): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L90)

Initialize Glean.

This should only be initialized once by the application, and not by
libraries using Glean. A message is logged to error and no changes are made
to the state if initialize is called a more than once.

A LifecycleObserver will be added to send pings when the application goes
into the background.

### Parameters

`applicationContext` - [Context](#) to access application features, such
as shared preferences

`configuration` - A Glean [Configuration](../../mozilla.components.service.glean.config/-configuration/index.md) object with global settings.