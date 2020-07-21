[android-components](../../index.md) / [mozilla.components.browser.session.storage](../index.md) / [AutoSave](index.md) / [periodicallyInForeground](./periodically-in-foreground.md)

# periodicallyInForeground

`fun periodicallyInForeground(interval: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 300, unit: `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)` = TimeUnit.SECONDS, scheduler: `[`ScheduledExecutorService`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/ScheduledExecutorService.html)` = Executors.newSingleThreadScheduledExecutor(), lifecycle: Lifecycle = ProcessLifecycleOwner.get().lifecycle): `[`AutoSave`](index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/storage/AutoSave.kt#L46)

Saves the state periodically when the app is in the foreground.

### Parameters

`interval` - The interval in which the state should be saved to disk.

`unit` - The time unit of the [interval](periodically-in-foreground.md#mozilla.components.browser.session.storage.AutoSave$periodicallyInForeground(kotlin.Long, java.util.concurrent.TimeUnit, java.util.concurrent.ScheduledExecutorService, androidx.lifecycle.Lifecycle)/interval) parameter.