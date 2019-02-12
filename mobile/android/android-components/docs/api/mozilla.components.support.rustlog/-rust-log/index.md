[android-components](../../index.md) / [mozilla.components.support.rustlog](../index.md) / [RustLog](./index.md)

# RustLog

`object RustLog` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/rustlog/src/main/java/mozilla/components/support/rustlog/RustLog.kt#L11)

### Functions

| Name | Summary |
|---|---|
| [disable](disable.md) | `fun disable(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Disable the rust log adapter. |
| [enable](enable.md) | `fun enable(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Enable the Rust log adapter. |
| [setMaxLevel](set-max-level.md) | `fun setMaxLevel(level: `[`Priority`](../../mozilla.components.support.base.log/-log/-priority/index.md)`, includePII: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Set the maximum level of logs that will be forwarded to [Log](../../mozilla.components.support.base.log/-log/index.md). By default, the max level is DEBUG. |
