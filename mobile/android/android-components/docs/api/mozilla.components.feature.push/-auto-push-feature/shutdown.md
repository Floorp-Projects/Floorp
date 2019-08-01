[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [shutdown](./shutdown.md)

# shutdown

`fun shutdown(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L109)

Overrides [PushProcessor.shutdown](../../mozilla.components.concept.push/-push-processor/shutdown.md)

Un-subscribes from all push message channels and stops the push service.
This should only be done on an account logout or app data deletion.

