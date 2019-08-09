[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [AutoPushFeature](index.md) / [forceRegistrationRenewal](./force-registration-renewal.md)

# forceRegistrationRenewal

`fun forceRegistrationRenewal(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L245)

Deletes the registration token locally so that it forces the service to get a new one the
next time hits it's messaging server.

Implementation notes: This shouldn't need to be used unless we're certain. When we introduce
[a polling service](https://github.com/mozilla-mobile/android-components/issues/3173) to check if endpoints are expired, we would invoke this.

