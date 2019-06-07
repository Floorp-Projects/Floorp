[android-components](../index.md) / [mozilla.components.service.fxa](index.md) / [handleFxaExceptions](./handle-fxa-exceptions.md)

# handleFxaExceptions

`fun <T> handleFxaExceptions(logger: `[`Logger`](../mozilla.components.support.base.log.logger/-logger/index.md)`, operation: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`T`](handle-fxa-exceptions.md#T)`, postHandleAuthErrorBlock: (`[`FxaUnauthorizedException`](-fxa-unauthorized-exception.md)`) -> `[`T`](handle-fxa-exceptions.md#T)`, handleErrorBlock: (`[`FxaException`](-fxa-exception.md)`) -> `[`T`](handle-fxa-exceptions.md#T)`): `[`T`](handle-fxa-exceptions.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Utils.kt#L21)

Runs a provided lambda, and if that throws non-panic, non-auth FxA exception, runs [handleErrorBlock](handle-fxa-exceptions.md#mozilla.components.service.fxa$handleFxaExceptions(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException.Unauthorized, mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException, mozilla.components.service.fxa.handleFxaExceptions.T)))/handleErrorBlock).
If that lambda throws an FxA auth exception, notifies [authErrorRegistry](../mozilla.components.service.fxa.manager/auth-error-registry.md), and runs [postHandleAuthErrorBlock](handle-fxa-exceptions.md#mozilla.components.service.fxa$handleFxaExceptions(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException.Unauthorized, mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException, mozilla.components.service.fxa.handleFxaExceptions.T)))/postHandleAuthErrorBlock).

### Parameters

`block` - A lambda to execute which mail fail with an [FxaException](-fxa-exception.md).

`postHandleAuthErrorBlock` - A lambda to execute if [block](handle-fxa-exceptions.md#mozilla.components.service.fxa$handleFxaExceptions(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException.Unauthorized, mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException, mozilla.components.service.fxa.handleFxaExceptions.T)))/block) failed with [FxaUnauthorizedException](-fxa-unauthorized-exception.md).

`handleErrorBlock` - A lambda to execute if [block](handle-fxa-exceptions.md#mozilla.components.service.fxa$handleFxaExceptions(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException.Unauthorized, mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException, mozilla.components.service.fxa.handleFxaExceptions.T)))/block) fails with a non-panic, non-auth [FxaException](-fxa-exception.md).

**Return**
object of type T, as defined by [block](handle-fxa-exceptions.md#mozilla.components.service.fxa$handleFxaExceptions(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException.Unauthorized, mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function1((mozilla.appservices.fxaclient.FxaException, mozilla.components.service.fxa.handleFxaExceptions.T)))/block).

`fun <T> handleFxaExceptions(logger: `[`Logger`](../mozilla.components.support.base.log.logger/-logger/index.md)`, operation: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, default: (`[`FxaException`](-fxa-exception.md)`) -> `[`T`](handle-fxa-exceptions.md#T)`, block: () -> `[`T`](handle-fxa-exceptions.md#T)`): `[`T`](handle-fxa-exceptions.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Utils.kt#L55)

Helper method that handles [FxaException](-fxa-exception.md) and allows specifying a lazy default value via [default](handle-fxa-exceptions.md#mozilla.components.service.fxa$handleFxaExceptions(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function1((mozilla.appservices.fxaclient.FxaException, mozilla.components.service.fxa.handleFxaExceptions.T)), kotlin.Function0((mozilla.components.service.fxa.handleFxaExceptions.T)))/default)
block for use in case of errors. Execution is wrapped in log statements.

`fun handleFxaExceptions(logger: `[`Logger`](../mozilla.components.support.base.log.logger/-logger/index.md)`, operation: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Utils.kt#L62)

Helper method that handles [FxaException](-fxa-exception.md) and returns a [Boolean](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) success flag as a result.

