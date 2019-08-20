[android-components](../index.md) / [mozilla.components.service.fxa.manager](index.md) / [authErrorRegistry](./auth-error-registry.md)

# authErrorRegistry

`val authErrorRegistry: `[`ObserverRegistry`](../mozilla.components.support.base.observer/-observer-registry/index.md)`<`[`AuthErrorObserver`](-auth-error-observer/index.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/manager/FxaAccountManager.kt#L61)

A global registry for propagating [AuthException](../mozilla.components.concept.sync/-auth-exception/index.md) errors. Components such as [SyncManager](../mozilla.components.service.fxa.sync/-sync-manager/index.md) and
[FxaDeviceRefreshManager](#) may encounter authentication problems during their normal operation, and
this registry is how they inform [FxaAccountManager](-fxa-account-manager/index.md) that these errors happened.

[FxaAccountManager](-fxa-account-manager/index.md) monitors this registry, adjusts internal state accordingly, and notifies
registered [AccountObserver](../mozilla.components.concept.sync/-account-observer/index.md) that account needs re-authentication.

