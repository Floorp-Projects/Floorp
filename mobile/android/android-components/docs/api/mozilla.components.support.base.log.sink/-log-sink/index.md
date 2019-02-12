[android-components](../../index.md) / [mozilla.components.support.base.log.sink](../index.md) / [LogSink](./index.md)

# LogSink

`interface LogSink` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/log/sink/LogSink.kt#L9)

### Functions

| Name | Summary |
|---|---|
| [log](log.md) | `abstract fun log(priority: `[`Priority`](../../mozilla.components.support.base.log/-log/-priority/index.md)` = Log.Priority.DEBUG, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`? = null, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| Name | Summary |
|---|---|
| [AndroidLogSink](../-android-log-sink/index.md) | `class AndroidLogSink : `[`LogSink`](./index.md)<br>LogSink implementation that writes to Android's log. |
