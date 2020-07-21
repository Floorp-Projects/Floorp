[android-components](../../../index.md) / [mozilla.components.service.fxa](../../index.md) / [AccountManagerException](../index.md) / [AuthRecoveryCircuitBreakerException](./index.md)

# AuthRecoveryCircuitBreakerException

`class AuthRecoveryCircuitBreakerException : `[`AccountManagerException`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Exceptions.kt#L41)

Hit a circuit-breaker during auth recovery flow.

### Parameters

`operation` - An operation which triggered an auth recovery flow that hit a circuit breaker.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AuthRecoveryCircuitBreakerException(operation: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Hit a circuit-breaker during auth recovery flow. |
