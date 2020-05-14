[android-components](../../index.md) / [mozilla.components.feature.accounts](../index.md) / [FxaWebChannelFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FxaWebChannelFeature(context: <ERROR CLASS>, customTabSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, runtime: `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, serverConfig: `[`ServerConfig`](../../mozilla.components.service.fxa/-server-config.md)`, fxaCapabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`FxaCapability`](../-fxa-capability/index.md)`> = emptySet())`

Feature implementation that provides Firefox Accounts WebChannel support.
For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
This feature uses a web extension to communicate with FxA Web Content.

