[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [SettingsMigrationException](./index.md)

# SettingsMigrationException

`class SettingsMigrationException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecSettingsMigrator.kt#L99)

Wraps [SettingsMigrationResult](../-settings-migration-result/index.md) in an exception so that it can be returned via [Result.Failure](../-result/-failure/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SettingsMigrationException(failure: `[`Failure`](../-settings-migration-result/-failure/index.md)`)`<br>Wraps [SettingsMigrationResult](../-settings-migration-result/index.md) in an exception so that it can be returned via [Result.Failure](../-result/-failure/index.md). |

### Properties

| Name | Summary |
|---|---|
| [failure](failure.md) | `val failure: `[`Failure`](../-settings-migration-result/-failure/index.md)<br>Wrapped [SettingsMigrationResult](../-settings-migration-result/index.md) indicating exact failure reason. |

### Extension Functions

| Name | Summary |
|---|---|
| [getStacktraceAsString](../../mozilla.components.support.base.ext/kotlin.-throwable/get-stacktrace-as-string.md) | `fun `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`.getStacktraceAsString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a formatted string of the [Throwable.stackTrace](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/stack-trace.html). |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [uniqueId](../java.lang.-exception/unique-id.md) | `fun `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`.uniqueId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a unique identifier for this [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) |
