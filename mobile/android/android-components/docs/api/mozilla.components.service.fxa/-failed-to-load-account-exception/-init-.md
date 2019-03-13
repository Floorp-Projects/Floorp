[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [FailedToLoadAccountException](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FailedToLoadAccountException(cause: `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`?)`

Propagated via [AccountObserver.onError](../../mozilla.components.concept.sync/-account-observer/on-error.md) if we fail to load a locally stored account during
initialization. No action is necessary from consumers.
Account state has been re-initialized.

### Parameters

`cause` - Optional original cause of failure.