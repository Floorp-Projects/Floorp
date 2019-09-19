[android-components](../../../index.md) / [mozilla.components.lib.crash](../../index.md) / [Crash](../index.md) / [NativeCodeCrash](./index.md)

# NativeCodeCrash

`data class NativeCodeCrash : `[`Crash`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/Crash.kt#L65)

A crash that happened in native code.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `NativeCodeCrash(minidumpPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, minidumpSuccess: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, extrasPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, isFatal: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../-breadcrumb/index.md)`>)`<br>A crash that happened in native code. |

### Properties

| Name | Summary |
|---|---|
| [breadcrumbs](breadcrumbs.md) | `val breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../-breadcrumb/index.md)`>` |
| [extrasPath](extras-path.md) | `val extrasPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Path to a file containing extra metadata about the crash. The file contains key-value pairs     in the form `Key=Value`. Be aware, it may contain sensitive data such as the URI that was     loaded at the time of the crash. |
| [isFatal](is-fatal.md) | `val isFatal: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Whether or not the crash was fatal or not: If true, the main application process was affected     by the crash. If false, only an internal process used by Gecko has crashed and the application     may be able to recover. |
| [minidumpPath](minidump-path.md) | `val minidumpPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Path to a Breakpad minidump file containing information about the crash. |
| [minidumpSuccess](minidump-success.md) | `val minidumpSuccess: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicating whether or not the crash dump was successfully retrieved. If this is false,     the dump file may be corrupted or incomplete. |
