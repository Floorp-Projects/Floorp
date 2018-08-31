---
title: AndroidLogSink - 
---

[mozilla.components.support.base.log.sink](../index.html) / [AndroidLogSink](./index.html)

# AndroidLogSink

`class AndroidLogSink : `[`LogSink`](../-log-sink/index.html)

LogSink implementation that writes to Android's log.

### Parameters

`defaultTag` - A default tag that should be used for all logging calls without tag.

### Constructors

| [&lt;init&gt;](-init-.html) | `AndroidLogSink(defaultTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "App")`<br>LogSink implementation that writes to Android's log. |

### Functions

| [log](log.html) | `fun log(priority: `[`Priority`](../../mozilla.components.support.base.log/-log/-priority/index.html)`, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`?, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Low-level logging call. |

