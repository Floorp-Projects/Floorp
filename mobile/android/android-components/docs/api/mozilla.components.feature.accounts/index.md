[android-components](../index.md) / [mozilla.components.feature.accounts](./index.md)

## Package mozilla.components.feature.accounts

### Types

| Name | Summary |
|---|---|
| [FirefoxAccountsAuthFeature](-firefox-accounts-auth-feature/index.md) | `class FirefoxAccountsAuthFeature`<br>Ties together an account manager with a session manager/tabs implementation, facilitating an authentication flow. |
| [FxaCapability](-fxa-capability/index.md) | `enum class FxaCapability`<br>Configurable FxA capabilities. |
| [FxaPushSupportFeature](-fxa-push-support-feature/index.md) | `class FxaPushSupportFeature`<br>A feature used for supporting FxA and push integration where needed. One of the main functions is when FxA notifies the device during a sync, that it's unable to reach the device via push messaging; triggering a push registration renewal. |
| [FxaWebChannelFeature](-fxa-web-channel-feature/index.md) | `class FxaWebChannelFeature : `[`SelectionAwareSessionObserver`](../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation that provides Firefox Accounts WebChannel support. For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md This feature uses a web extension to communicate with FxA Web Content. |
