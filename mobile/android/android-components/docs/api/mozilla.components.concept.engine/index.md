[android-components](../index.md) / [mozilla.components.concept.engine](./index.md)

## Package mozilla.components.concept.engine

### Types

| Name | Summary |
|---|---|
| [CancellableOperation](-cancellable-operation/index.md) | `interface CancellableOperation`<br>Represents an async operation that can be cancelled. |
| [DataCleanable](-data-cleanable/index.md) | `interface DataCleanable`<br>Contract to indicate how objects with the ability to clear data should behave. |
| [DefaultSettings](-default-settings/index.md) | `data class DefaultSettings : `[`Settings`](-settings/index.md)<br>[Settings](-settings/index.md) implementation used to set defaults for [Engine](-engine/index.md) and [EngineSession](-engine-session/index.md). |
| [Engine](-engine/index.md) | `interface Engine : `[`WebExtensionRuntime`](../mozilla.components.concept.engine.webextension/-web-extension-runtime/index.md)`, `[`DataCleanable`](-data-cleanable/index.md)<br>Entry point for interacting with the engine implementation. |
| [EngineSession](-engine-session/index.md) | `abstract class EngineSession : `[`Observable`](../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](-engine-session/-observer/index.md)`>, `[`DataCleanable`](-data-cleanable/index.md)<br>Class representing a single engine session. |
| [EngineSessionState](-engine-session-state/index.md) | `interface EngineSessionState`<br>The state of an [EngineSession](-engine-session/index.md). An instance can be obtained from [EngineSession.saveState](-engine-session/save-state.md). Creating a new [EngineSession](-engine-session/index.md) and calling [EngineSession.restoreState](-engine-session/restore-state.md) with the same state instance should restore the previous session. |
| [EngineView](-engine-view/index.md) | `interface EngineView`<br>View component that renders web content. |
| [HitResult](-hit-result/index.md) | `sealed class HitResult`<br>Represents all the different supported types of data that can be found from long clicking an element. |
| [LifecycleObserver](-lifecycle-observer/index.md) | `class LifecycleObserver : LifecycleObserver`<br>[LifecycleObserver](-lifecycle-observer/index.md) which dispatches lifecycle events to an [EngineView](-engine-view/index.md). |
| [Settings](-settings/index.md) | `abstract class Settings`<br>Holds settings of an engine or session. Concrete engine implementations define how these settings are applied i.e. whether a setting is applied on an engine or session instance. |
| [UnsupportedSetting](-unsupported-setting/index.md) | `class UnsupportedSetting<T>` |

### Exceptions

| Name | Summary |
|---|---|
| [UnsupportedSettingException](-unsupported-setting-exception/index.md) | `class UnsupportedSettingException : `[`RuntimeException`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-runtime-exception/index.html)<br>Exception thrown by default if a setting is not supported by an engine or session. |
