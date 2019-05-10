[android-components](../index.md) / [mozilla.components.service.fxa](index.md) / [maybeExceptional](./maybe-exceptional.md)

# maybeExceptional

`fun <T> maybeExceptional(block: () -> `[`T`](maybe-exceptional.md#T)`, handleErrorBlock: (`[`FxaException`](-fxa-exception.md)`) -> `[`T`](maybe-exceptional.md#T)`): `[`T`](maybe-exceptional.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Utils.kt#L14)

Runs a provided lambda, and if that throws non-panic FxA exception, runs the second lambda.

### Parameters

`block` - A lambda to execute which mail fail with an [FxaException](-fxa-exception.md).

`handleErrorBlock` - A lambda to execute if [block](maybe-exceptional.md#mozilla.components.service.fxa$maybeExceptional(kotlin.Function0((mozilla.components.service.fxa.maybeExceptional.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException, mozilla.components.service.fxa.maybeExceptional.T)))/block) fails with a non-panic [FxaException](-fxa-exception.md).

**Return**
object of type T, as defined by [block](maybe-exceptional.md#mozilla.components.service.fxa$maybeExceptional(kotlin.Function0((mozilla.components.service.fxa.maybeExceptional.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException, mozilla.components.service.fxa.maybeExceptional.T)))/block).

