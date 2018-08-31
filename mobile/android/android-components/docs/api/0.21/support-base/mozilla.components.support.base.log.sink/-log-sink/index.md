---
title: LogSink - 
---

[mozilla.components.support.base.log.sink](../index.html) / [LogSink](./index.html)

# LogSink

`interface LogSink`

### Functions

| [log](log.html) | `abstract fun log(priority: `[`Priority`](../../mozilla.components.support.base.log/-log/-priority/index.html)` = Log.Priority.DEBUG, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`? = null, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inheritors

| [AndroidLogSink](../-android-log-sink/index.html) | `class AndroidLogSink : `[`LogSink`](./index.md)<br>LogSink implementation that writes to Android's log. |

