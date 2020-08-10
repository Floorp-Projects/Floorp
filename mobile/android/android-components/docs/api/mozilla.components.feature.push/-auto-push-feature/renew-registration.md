[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [renewRegistration](./renew-registration.md)

# renewRegistration

`fun renewRegistration(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L263)

Overrides [PushProcessor.renewRegistration](../../mozilla.components.concept.push/-push-processor/renew-registration.md)

Deletes the registration token locally so that it forces the service to get a new one the
next time hits it's messaging server.

