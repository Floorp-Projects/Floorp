[android-components](../../index.md) / [mozilla.components.feature.accounts](../index.md) / [FxaWebChannelFeature](./index.md)

# FxaWebChannelFeature

`class FxaWebChannelFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/accounts/src/main/java/mozilla/components/feature/accounts/FxaWebChannelFeature.kt#L58)

Feature implementation that provides Firefox Accounts WebChannel support.
For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
This feature uses a web extension to communicate with FxA Web Content.

### Types

| Name | Summary |
|---|---|
| [WebChannelCommand](-web-channel-command/index.md) | `enum class WebChannelCommand` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FxaWebChannelFeature(context: <ERROR CLASS>, customTabSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, runtime: `[`WebExtensionRuntime`](../../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`, serverConfig: `[`ServerConfig`](../../mozilla.components.service.fxa/-server-config.md)`, fxaCapabilities: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`FxaCapability`](../-fxa-capability/index.md)`> = emptySet())`<br>Feature implementation that provides Firefox Accounts WebChannel support. For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md This feature uses a web extension to communicate with FxA Web Content. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
