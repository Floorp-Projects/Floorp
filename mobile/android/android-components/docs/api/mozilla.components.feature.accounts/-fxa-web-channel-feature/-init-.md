[android-components](../../index.md) / [mozilla.components.feature.accounts](../index.md) / [FxaWebChannelFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FxaWebChannelFeature(context: <ERROR CLASS>, customTabSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, fxaCapabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`FxaCapability`](../-fxa-capability/index.md)`> = emptySet())`

Feature implementation that provides Firefox Accounts WebChannel support.
For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
This feature uses a web extension to communicate with FxA Web Content.

