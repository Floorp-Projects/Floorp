[android-components](../../index.md) / [mozilla.components.support.rustlog](../index.md) / [RustLog](index.md) / [setMaxLevel](./set-max-level.md)

# setMaxLevel

`fun setMaxLevel(level: `[`Priority`](../../mozilla.components.support.base.log/-log/-priority/index.md)`, includePII: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/rustlog/src/main/java/mozilla/components/support/rustlog/RustLog.kt#L70)

Set the maximum level of logs that will be forwarded to [Log](../../mozilla.components.support.base.log/-log/index.md). By
default, the max level is DEBUG.

This is somewhat redundant with [Log.logLevel](../../mozilla.components.support.base.log/-log/log-level.md) (and a stricter
filter on Log.logLevel will take precedence here), however
setting the max level here can improve performance a great deal,
as it allows the Rust code to skip a great deal of work.

This includes a `includePII` flag, which allows enabling logs at
the trace level. It is ignored if level is not [Log.Priority.DEBUG](../../mozilla.components.support.base.log/-log/-priority/-d-e-b-u-g.md).
These trace level logs* may contain the personal information of users
but can be very helpful for tracking down bugs.

### Parameters

`level` - The maximum (inclusive) level to include logs at.

`includePII` - If `level` is [Log.Priority.DEBUG](../../mozilla.components.support.base.log/-log/-priority/-d-e-b-u-g.md), allow
    debug logs to contain PII.