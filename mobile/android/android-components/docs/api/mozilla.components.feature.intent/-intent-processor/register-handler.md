[android-components](../../index.md) / [mozilla.components.feature.intent](../index.md) / [IntentProcessor](index.md) / [registerHandler](./register-handler.md)

# registerHandler

`fun registerHandler(action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, handler: `[`IntentHandler`](../-intent-handler.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/intent/src/main/java/mozilla/components/feature/intent/IntentProcessor.kt#L88)

Registers the given handler to be invoked for intents with the given action. If a
handler is already present it will be overwritten.

### Parameters

`action` - the intent action which should trigger the provided handler.

`handler` - the intent handler to be registered.