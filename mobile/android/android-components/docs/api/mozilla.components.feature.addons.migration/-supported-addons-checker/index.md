[android-components](../../index.md) / [mozilla.components.feature.addons.migration](../index.md) / [SupportedAddonsChecker](./index.md)

# SupportedAddonsChecker

`interface SupportedAddonsChecker` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/migration/SupportedAddonsChecker.kt#L42)

Contract to define the behavior for a periodic checker for newly supported add-ons.

### Types

| Name | Summary |
|---|---|
| [Frequency](-frequency/index.md) | `data class Frequency`<br>Indicates how often checks for newly supported add-ons should happen. |

### Functions

| Name | Summary |
|---|---|
| [registerForChecks](register-for-checks.md) | `abstract fun registerForChecks(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Registers for periodic checks for new available add-ons. |
| [unregisterForChecks](unregister-for-checks.md) | `abstract fun unregisterForChecks(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unregisters for periodic checks for new available add-ons. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [DefaultSupportedAddonsChecker](../-default-supported-addons-checker/index.md) | `class DefaultSupportedAddonsChecker : `[`SupportedAddonsChecker`](./index.md)<br>An implementation of [SupportedAddonsChecker](./index.md) that uses the work manager api for scheduling checks. |
