[android-components](../../index.md) / [mozilla.components.lib.nearby](../index.md) / [NearbyConnection](index.md) / [sendMessage](./send-message.md)

# sendMessage

`fun sendMessage(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/nearby/src/main/java/mozilla/components/lib/nearby/NearbyConnection.kt#L386)

Sends a message to a connected device. If the current state is not
[ConnectionState.ReadyToSend](-connection-state/-ready-to-send/index.md), the message will not be sent.

### Parameters

`message` - the message to send

**Return**
an id that will be later passed back through
[NearbyConnectionObserver.onMessageDelivered](../-nearby-connection-observer/on-message-delivered.md), or null if the message could not be sent

