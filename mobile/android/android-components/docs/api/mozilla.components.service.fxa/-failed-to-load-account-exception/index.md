[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FailedToLoadAccountException](./index.md)

# FailedToLoadAccountException

`class FailedToLoadAccountException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/FxaAccountManager.kt#L74)

Propagated via [AccountObserver.onError](../../mozilla.components.concept.sync/-account-observer/on-error.md) if we fail to load a locally stored account during
initialization. No action is necessary from consumers.
Account state has been re-initialized.

### Parameters

`cause` - Optional original cause of failure.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `FailedToLoadAccountException(cause: `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`?)`<br>Propagated via [AccountObserver.onError](../../mozilla.components.concept.sync/-account-observer/on-error.md) if we fail to load a locally stored account during initialization. No action is necessary from consumers. Account state has been re-initialized. |
