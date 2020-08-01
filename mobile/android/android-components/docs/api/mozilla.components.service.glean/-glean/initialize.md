[android-components](../../index.md) / [mozilla.components.service.glean](../index.md) / [Glean](index.md) / [initialize](./initialize.md)

# initialize

`@MainThread fun initialize(applicationContext: <ERROR CLASS>, uploadEnabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, configuration: `[`Configuration`](../../mozilla.components.service.glean.config/-configuration/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/Glean.kt#L37)

Initialize Glean.

This should only be initialized once by the application, and not by
libraries using Glean. A message is logged to error and no changes are made
to the state if initialize is called a more than once.

A LifecycleObserver will be added to send pings when the application goes
into the background.

### Parameters

`applicationContext` - [Context](#) to access application features, such
as shared preferences

`uploadEnabled` - A [Boolean](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) that determines the initial state of the uploader

`configuration` - A Glean [Configuration](../../mozilla.components.service.glean.config/-configuration/index.md) object with global settings.