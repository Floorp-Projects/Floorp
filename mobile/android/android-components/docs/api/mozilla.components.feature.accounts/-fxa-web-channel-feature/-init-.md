[android-components](../../index.md) / [mozilla.components.feature.accounts](../index.md) / [FxaWebChannelFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FxaWebChannelFeature(context: <ERROR CLASS>, customTabSessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, accountManager: `[`FxaAccountManager`](../../mozilla.components.service.fxa.manager/-fxa-account-manager/index.md)`)`

Feature implementation that provides Firefox Accounts WebChannel support.
For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
This feature uses a web extension to communicate with FxA Web Content.

Boilerplate around installing and communicating with the extension will be cleaned up as part https://github.com/mozilla-mobile/android-components/issues/4297.

