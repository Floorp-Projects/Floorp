[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [AccountManagerException](./index.md)

# AccountManagerException

`sealed class AccountManagerException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Exceptions.kt#L36)

Exceptions related to the account manager.

### Exceptions

| Name | Summary |
|---|---|
| [AuthRecoveryCircuitBreakerException](-auth-recovery-circuit-breaker-exception/index.md) | `class AuthRecoveryCircuitBreakerException : `[`AccountManagerException`](./index.md)<br>Hit a circuit-breaker during auth recovery flow. |

### Extension Functions

| Name | Summary |
|---|---|
| [getStacktraceAsString](../../mozilla.components.support.base.ext/kotlin.-throwable/get-stacktrace-as-string.md) | `fun `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`.getStacktraceAsString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a formatted string of the [Throwable.stackTrace](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/stack-trace.html). |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [uniqueId](../../mozilla.components.support.migration/java.lang.-exception/unique-id.md) | `fun `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`.uniqueId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a unique identifier for this [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [AuthRecoveryCircuitBreakerException](-auth-recovery-circuit-breaker-exception/index.md) | `class AuthRecoveryCircuitBreakerException : `[`AccountManagerException`](./index.md)<br>Hit a circuit-breaker during auth recovery flow. |
