[android-components](../../index.md) / [mozilla.components.feature.addons.migration](../index.md) / [DefaultSupportedAddonsChecker](./index.md)

# DefaultSupportedAddonsChecker

`class DefaultSupportedAddonsChecker : `[`SupportedAddonsChecker`](../-supported-addons-checker/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/migration/SupportedAddonsChecker.kt#L70)

An implementation of [SupportedAddonsChecker](../-supported-addons-checker/index.md) that uses the work manager api for scheduling checks.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `DefaultSupportedAddonsChecker(applicationContext: <ERROR CLASS>, frequency: `[`Frequency`](../-supported-addons-checker/-frequency/index.md)` = Frequency(1, TimeUnit.DAYS), onNotificationClickIntent: <ERROR CLASS> = createDefaultNotificationIntent(applicationContext))`<br>An implementation of [SupportedAddonsChecker](../-supported-addons-checker/index.md) that uses the work manager api for scheduling checks. |

### Functions

| Name | Summary |
|---|---|
| [registerForChecks](register-for-checks.md) | `fun registerForChecks(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [SupportedAddonsChecker.registerForChecks](../-supported-addons-checker/register-for-checks.md) |
| [unregisterForChecks](unregister-for-checks.md) | `fun unregisterForChecks(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>See [SupportedAddonsChecker.unregisterForChecks](../-supported-addons-checker/unregister-for-checks.md) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
