[android-components](../index.md) / [mozilla.components.feature.session](./index.md)

## Package mozilla.components.feature.session

### Types

| Name | Summary |
|---|---|
| [CoordinateScrollingFeature](-coordinate-scrolling-feature/index.md) | `class CoordinateScrollingFeature : `[`SelectionAwareSessionObserver`](../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation for connecting an [EngineView](../mozilla.components.concept.engine/-engine-view/index.md) with any View that you want to coordinate scrolling behavior with. |
| [FullScreenFeature](-full-screen-feature/index.md) | `open class FullScreenFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../mozilla.components.support.base.feature/-user-interaction-handler/index.md)<br>Feature implementation for handling fullscreen mode (exiting and back button presses). |
| [HistoryDelegate](-history-delegate/index.md) | `class HistoryDelegate : `[`HistoryTrackingDelegate`](../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md)<br>Implementation of the [HistoryTrackingDelegate](../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md) which delegates work to an instance of [HistoryStorage](../mozilla.components.concept.storage/-history-storage/index.md). |
| [PictureInPictureFeature](-picture-in-picture-feature/index.md) | `class PictureInPictureFeature`<br>A simple implementation of Picture-in-picture mode if on a supported platform. |
| [SessionFeature](-session-feature/index.md) | `class SessionFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../mozilla.components.support.base.feature/-user-interaction-handler/index.md)<br>Feature implementation for connecting the engine module with the session module. |
| [SessionUseCases](-session-use-cases/index.md) | `class SessionUseCases`<br>Contains use cases related to the session feature. |
| [SettingsUseCases](-settings-use-cases/index.md) | `class SettingsUseCases`<br>Contains use cases related to engine [Settings](../mozilla.components.concept.engine/-settings/index.md). |
| [SwipeRefreshFeature](-swipe-refresh-feature/index.md) | `class SwipeRefreshFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, OnChildScrollUpCallback, OnRefreshListener`<br>Feature implementation to add pull to refresh functionality to browsers. |
| [TrackingProtectionUseCases](-tracking-protection-use-cases/index.md) | `class TrackingProtectionUseCases`<br>Contains use cases related to the tracking protection. |
