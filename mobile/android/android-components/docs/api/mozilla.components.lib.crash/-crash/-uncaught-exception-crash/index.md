[android-components](../../../index.md) / [mozilla.components.lib.crash](../../index.md) / [Crash](../index.md) / [UncaughtExceptionCrash](./index.md)

# UncaughtExceptionCrash

`data class UncaughtExceptionCrash : `[`Crash`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/Crash.kt#L44)

A crash caused by an uncaught exception.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UncaughtExceptionCrash(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`, breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../../mozilla.components.support.base.crash/-breadcrumb/index.md)`>, uuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = UUID.randomUUID().toString())`<br>A crash caused by an uncaught exception. |

### Properties

| Name | Summary |
|---|---|
| [breadcrumbs](breadcrumbs.md) | `val breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../../mozilla.components.support.base.crash/-breadcrumb/index.md)`>`<br>List of breadcrumbs to send with the crash report. |
| [throwable](throwable.md) | `val throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)<br>The [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) that caused the crash. |
| [uuid](uuid.md) | `val uuid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Unique ID identifying this crash. |
