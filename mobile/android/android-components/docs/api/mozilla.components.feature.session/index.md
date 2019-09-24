[android-components](../index.md) / [mozilla.components.feature.session](./index.md)

## Package mozilla.components.feature.session

### Types

| Name | Summary |
|---|---|
| [CoordinateScrollingFeature](-coordinate-scrolling-feature/index.md) | `class CoordinateScrollingFeature : `[`SelectionAwareSessionObserver`](../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation for connecting an [EngineView](../mozilla.components.concept.engine/-engine-view/index.md) with any View that you want to coordinate scrolling behavior with. |
| [EngineViewPresenter](-engine-view-presenter/index.md) | `class EngineViewPresenter : `[`Observer`](../mozilla.components.browser.session/-session-manager/-observer/index.md)<br>Presenter implementation for EngineView. |
| [FullScreenFeature](-full-screen-feature/index.md) | `open class FullScreenFeature : `[`SelectionAwareSessionObserver`](../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../mozilla.components.support.base.feature/-back-handler/index.md)<br>Feature implementation for handling fullscreen mode (exiting and back button presses). |
| [HistoryDelegate](-history-delegate/index.md) | `class HistoryDelegate : `[`HistoryTrackingDelegate`](../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md)<br>Implementation of the [HistoryTrackingDelegate](../mozilla.components.concept.engine.history/-history-tracking-delegate/index.md) which delegates work to an instance of [HistoryStorage](../mozilla.components.concept.storage/-history-storage/index.md). |
| [PictureInPictureFeature](-picture-in-picture-feature/index.md) | `class PictureInPictureFeature`<br>A simple implementation of Picture-in-picture mode if on a supported platform. |
| [SessionFeature](-session-feature/index.md) | `class SessionFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`BackHandler`](../mozilla.components.support.base.feature/-back-handler/index.md)<br>Feature implementation for connecting the engine module with the session module. |
| [SessionUseCases](-session-use-cases/index.md) | `class SessionUseCases`<br>Contains use cases related to the session feature. |
| [SettingsUseCases](-settings-use-cases/index.md) | `class SettingsUseCases`<br>Contains use cases related to user settings. |
| [SwipeRefreshFeature](-swipe-refresh-feature/index.md) | `class SwipeRefreshFeature : `[`SelectionAwareSessionObserver`](../mozilla.components.browser.session/-selection-aware-session-observer/index.md)`, `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, OnChildScrollUpCallback, OnRefreshListener`<br>Feature implementation to add pull to refresh functionality to browsers. |
| [ThumbnailsFeature](-thumbnails-feature/index.md) | `class ThumbnailsFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation for automatically taking thumbnails of sites. The feature will take a screenshot when the page finishes loading, and will add it to the [Session.thumbnail](../mozilla.components.browser.session/-session/thumbnail.md) property. |
| [TrackingProtectionUseCases](-tracking-protection-use-cases/index.md) | `class TrackingProtectionUseCases`<br>Contains use cases related to the tracking protection. |
| [WindowFeature](-window-feature/index.md) | `class WindowFeature : `[`LifecycleAwareFeature`](../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)<br>Feature implementation for handling window requests. |
