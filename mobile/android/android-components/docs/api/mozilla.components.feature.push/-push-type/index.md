[android-components](../../index.md) / [mozilla.components.feature.push](../index.md) / [PushType](./index.md)

# PushType

`enum class PushType` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/push/src/main/java/mozilla/components/feature/push/AutoPushFeature.kt#L287)

The different kind of message types that a [EncryptedPushMessage](../../mozilla.components.concept.push/-encrypted-push-message/index.md) can be:

* Application Services (e.g. FxA/Send Tab)
* WebPush messages (see: https://www.w3.org/TR/push-api/)

### Enum Values

| Name | Summary |
|---|---|
| [Services](-services.md) |  |
| [WebPush](-web-push.md) |  |

### Functions

| Name | Summary |
|---|---|
| [toChannelId](to-channel-id.md) | `fun toChannelId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Retrieve a channel ID from the PushType. |
