[android-components](../../index.md) / [mozilla.components.feature.privatemode.notification](../index.md) / [AbstractPrivateNotificationService](index.md) / [onCreate](./on-create.md)

# onCreate

`@ExperimentalCoroutinesApi fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/privatemode/src/main/java/mozilla/components/feature/privatemode/notification/AbstractPrivateNotificationService.kt#L70)

Create the private browsing notification and
add a listener to stop the service once all private tabs are closed.

The service should be started only if private tabs are open.

