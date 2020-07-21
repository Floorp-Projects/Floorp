[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [UnsupportedSettingException](./index.md)

# UnsupportedSettingException

`class UnsupportedSettingException : `[`RuntimeException`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-runtime-exception/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/Settings.kt#L229)

Exception thrown by default if a setting is not supported by an engine or session.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UnsupportedSettingException(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "Setting not supported by this engine")`<br>Exception thrown by default if a setting is not supported by an engine or session. |

### Extension Functions

| Name | Summary |
|---|---|
| [getStacktraceAsString](../../mozilla.components.support.base.ext/kotlin.-throwable/get-stacktrace-as-string.md) | `fun `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`.getStacktraceAsString(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a formatted string of the [Throwable.stackTrace](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/stack-trace.html). |
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [uniqueId](../../mozilla.components.support.migration/java.lang.-exception/unique-id.md) | `fun `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)`.uniqueId(): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Returns a unique identifier for this [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) |
