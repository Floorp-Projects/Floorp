---
title: SessionIntentProcessor.registerHandler - 
---

[mozilla.components.feature.session](../index.html) / [SessionIntentProcessor](index.html) / [registerHandler](./register-handler.html)

# registerHandler

`fun registerHandler(action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, handler: `[`IntentHandler`](../-intent-handler.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Registers the given handler to be invoked for intents with the given action. If a
handler is already present it will be overwritten.

### Parameters

`action` - the intent action which should trigger the provided handler.

`handler` - the intent handler to be registered.