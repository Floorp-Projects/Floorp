[android-components](../../index.md) / [mozilla.components.lib.crash](../index.md) / [Crash](./index.md)

# Crash

`sealed class Crash` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/Crash.kt#L32)

Crash types that are handled by this library.

### Types

| Name | Summary |
|---|---|
| [NativeCodeCrash](-native-code-crash/index.md) | `data class NativeCodeCrash : `[`Crash`](./index.md)<br>A crash that happened in native code. |
| [UncaughtExceptionCrash](-uncaught-exception-crash/index.md) | `data class UncaughtExceptionCrash : `[`Crash`](./index.md)<br>A crash caused by an uncaught exception. |

### Properties

| Name | Summary |
|---|---|
| [uuid](uuid.md) | `abstract val uuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Unique ID identifying this crash. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [fromIntent](from-intent.md) | `fun fromIntent(intent: <ERROR CLASS>): `[`Crash`](./index.md) |
| [isCrashIntent](is-crash-intent.md) | `fun isCrashIntent(intent: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [NativeCodeCrash](-native-code-crash/index.md) | `data class NativeCodeCrash : `[`Crash`](./index.md)<br>A crash that happened in native code. |
| [UncaughtExceptionCrash](-uncaught-exception-crash/index.md) | `data class UncaughtExceptionCrash : `[`Crash`](./index.md)<br>A crash caused by an uncaught exception. |
