[android-components](../../index.md) / [mozilla.components.concept.engine](../index.md) / [LifecycleObserver](./index.md)

# LifecycleObserver

`class LifecycleObserver : LifecycleObserver` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineView.kt#L103)

[LifecycleObserver](./index.md) which dispatches lifecycle events to an [EngineView](../-engine-view/index.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LifecycleObserver(engineView: `[`EngineView`](../-engine-view/index.md)`)`<br>[LifecycleObserver](./index.md) which dispatches lifecycle events to an [EngineView](../-engine-view/index.md). |

### Properties

| Name | Summary |
|---|---|
| [engineView](engine-view.md) | `val engineView: `[`EngineView`](../-engine-view/index.md) |

### Functions

| Name | Summary |
|---|---|
| [onCreate](on-create.md) | `fun onCreate(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDestroy](on-destroy.md) | `fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onPause](on-pause.md) | `fun onPause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onResume](on-resume.md) | `fun onResume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStart](on-start.md) | `fun onStart(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onStop](on-stop.md) | `fun onStop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
