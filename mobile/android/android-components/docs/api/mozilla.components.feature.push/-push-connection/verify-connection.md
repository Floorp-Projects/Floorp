[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushConnection](index.md) / [verifyConnection](./verify-connection.md)

# verifyConnection

`abstract suspend fun verifyConnection(): `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`AutoPushSubscriptionChanged`](../-auto-push-subscription-changed/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/Connection.kt#L70)

Checks validity of current push subscriptions.

Implementation notes: This API will change to return the specific subscriptions that have been updated.
See: https://github.com/mozilla/application-services/issues/2049

**Return**
the list of push subscriptions that were updated for subscribers that should be notified about.

