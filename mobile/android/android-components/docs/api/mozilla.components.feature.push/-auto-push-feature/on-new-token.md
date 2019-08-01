[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [onNewToken](./on-new-token.md)

# onNewToken

`fun onNewToken(newToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L127)

Overrides [PushProcessor.onNewToken](../../mozilla.components.concept.push/-push-processor/on-new-token.md)

New registration tokens are received and sent to the Autopush server which also performs subscriptions for
each push type and notifies the subscribers.

