[android-components](../../../index.md) / [mozilla.components.lib.crash](../../index.md) / [Crash](../index.md) / [UncaughtExceptionCrash](./index.md)

# UncaughtExceptionCrash

`data class UncaughtExceptionCrash : `[`Crash`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/Crash.kt#L35)

A crash caused by an uncaught exception.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `UncaughtExceptionCrash(throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)`, breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../-breadcrumb/index.md)`>)`<br>A crash caused by an uncaught exception. |

### Properties

| Name | Summary |
|---|---|
| [breadcrumbs](breadcrumbs.md) | `val breadcrumbs: `[`ArrayList`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-array-list/index.html)`<`[`Breadcrumb`](../../-breadcrumb/index.md)`>` |
| [throwable](throwable.md) | `val throwable: `[`Throwable`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html)<br>The [Throwable](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-throwable/index.html) that caused the crash. |
