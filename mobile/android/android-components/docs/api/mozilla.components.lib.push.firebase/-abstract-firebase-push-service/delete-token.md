[android-components](../../index.md) / [mozilla.components.lib.push.firebase](../index.md) / [AbstractFirebasePushService](index.md) / [deleteToken](./delete-token.md)

# deleteToken

`open fun deleteToken(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/push-firebase/src/main/java/mozilla/components/lib/push/firebase/AbstractFirebasePushService.kt#L80)

Overrides [PushService.deleteToken](../../mozilla.components.concept.push/-push-service/delete-token.md)

Removes the Firebase instance ID. This would lead a new token being generated when the
service hits the Firebase servers.

