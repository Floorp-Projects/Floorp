[android-components](../../index.md) / [mozilla.components.browser.session.storage](../index.md) / [AutoSave](./index.md)

# AutoSave

`class AutoSave` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/storage/AutoSave.kt#L27)

### Types

| Name | Summary |
|---|---|
| [Storage](-storage/index.md) | `interface Storage` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `AutoSave(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, sessionStorage: `[`Storage`](-storage/index.md)`, minimumIntervalMs: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`)` |

### Functions

| Name | Summary |
|---|---|
| [periodicallyInForeground](periodically-in-foreground.md) | `fun periodicallyInForeground(interval: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 300, unit: `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)` = TimeUnit.SECONDS, scheduler: `[`ScheduledExecutorService`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/ScheduledExecutorService.html)` = Executors.newSingleThreadScheduledExecutor(), lifecycle: Lifecycle = ProcessLifecycleOwner.get().lifecycle): `[`AutoSave`](./index.md)<br>Saves the state periodically when the app is in the foreground. |
| [whenGoingToBackground](when-going-to-background.md) | `fun whenGoingToBackground(lifecycle: Lifecycle = ProcessLifecycleOwner.get().lifecycle): `[`AutoSave`](./index.md)<br>Saves the state automatically when the app goes to the background. |
| [whenSessionsChange](when-sessions-change.md) | `fun whenSessionsChange(): `[`AutoSave`](./index.md)<br>Saves the state automatically when the sessions change, e.g. sessions get added and removed. |

### Companion Object Properties

| Name | Summary |
|---|---|
| [DEFAULT_INTERVAL_MILLISECONDS](-d-e-f-a-u-l-t_-i-n-t-e-r-v-a-l_-m-i-l-l-i-s-e-c-o-n-d-s.md) | `const val DEFAULT_INTERVAL_MILLISECONDS: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
