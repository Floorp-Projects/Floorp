[android-components](../../index.md) / [mozilla.components.support.rustlog](../index.md) / [RustLog](./index.md)

# RustLog

`object RustLog` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/rustlog/src/main/java/mozilla/components/support/rustlog/RustLog.kt#L16)

### Functions

| Name | Summary |
|---|---|
| [disable](disable.md) | `fun disable(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disable the rust log adapter. |
| [enable](enable.md) | `fun enable(crashReporter: `[`CrashReporting`](../../mozilla.components.support.base.crash/-crash-reporting/index.md)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enable the Rust log adapter. |
| [setMaxLevel](set-max-level.md) | `fun setMaxLevel(level: `[`Priority`](../../mozilla.components.support.base.log/-log/-priority/index.md)`, includePII: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set the maximum level of logs that will be forwarded to [Log](../../mozilla.components.support.base.log/-log/index.md). By default, the max level is DEBUG. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
