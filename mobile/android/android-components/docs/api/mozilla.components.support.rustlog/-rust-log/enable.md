[android-components](../../index.md) / [mozilla.components.support.rustlog](../index.md) / [RustLog](index.md) / [enable](./enable.md)

# enable

`fun enable(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/rustlog/src/main/java/mozilla/components/support/rustlog/RustLog.kt#L33)

Enable the Rust log adapter.

This does almost (see below) nothing if you are not in a megazord build.
After calling this, logs emitted by Rust code are forwarded to any
LogSinks attached to [Log](../../mozilla.components.support.base.log/-log/index.md).

Megazording is required due to each dynamically loaded Rust library having
its own internal/private version of the Rust logging framework. When
megazording, this is still true, but there's only a single dynamically
loaded library, and so it is redirected properly.

Note that non-megazord versions of the Rust libraries will log directly to
logcat by default (at DEBUG level), so while they cannot hook into the base
component log system, they still have logs available for development use.

(We say "almost" nothing, as calling this will hook up logging for the dynamic
library containing the Rust log hooking (and only that), as well as logging
a single message indicating that it completed initialization).

