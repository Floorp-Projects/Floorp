[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [kotlin.String](index.md) / [toAuthType](./to-auth-type.md)

# toAuthType

`fun `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?.toAuthType(): `[`AuthType`](../../mozilla.components.concept.sync/-auth-type/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Types.kt#L29)

Converts a raw 'action' string into an [AuthType](../../mozilla.components.concept.sync/-auth-type/index.md) instance.
Actions come to us from FxA during an OAuth login, either over the WebChannel or via the redirect URL.

