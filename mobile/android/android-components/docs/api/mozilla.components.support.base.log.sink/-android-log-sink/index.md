[android-components](../../index.md) / [mozilla.components.support.base.log.sink](../index.md) / [AndroidLogSink](./index.md)

# AndroidLogSink

`class AndroidLogSink : `[`LogSink`](../-log-sink/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/log/sink/AndroidLogSink.kt#L18)

LogSink implementation that writes to Android's log.

### Parameters

`defaultTag` - A default tag that should be used for all logging calls without tag.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AndroidLogSink(defaultTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "App")`<br>LogSink implementation that writes to Android's log. |

### Functions

| Name | Summary |
|---|---|
| [log](log.md) | `fun log(priority: `[`Priority`](../../mozilla.components.support.base.log/-log/-priority/index.md)`, tag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?, throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`?, message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Low-level logging call. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
