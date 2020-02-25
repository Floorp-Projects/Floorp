[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushConnection](index.md) / [verifyConnection](./verify-connection.md)

# verifyConnection

`abstract suspend fun verifyConnection(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/Connection.kt#L64)

Checks validity of current push subscriptions.

Implementation notes: This API will change to return the specific subscriptions that have been updated.
See: https://github.com/mozilla/application-services/issues/2049

**Return**
true if a push subscription was updated and a subscriber should be notified of the change.

