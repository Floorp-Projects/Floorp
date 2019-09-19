[android-components](../../index.md) / [mozilla.components.support.webextensions](../index.md) / [WebExtensionController](index.md) / [install](./install.md)

# install

`fun install(engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/webextensions/src/main/java/mozilla/components/support/webextensions/WebExtensionController.kt#L38)

Makes sure the web extension is installed in the provided engine. If a
content message handler was registered (see
[registerContentMessageHandler](register-content-message-handler.md)) before install completed, registration
will happen upon successful installation.

### Parameters

`engine` - the [Engine](../../mozilla.components.concept.engine/-engine/index.md) the web extension should be installed in.