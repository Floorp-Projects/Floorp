[android-components](../../index.md) / [mozilla.components.feature.readerview](../index.md) / [ReaderViewFeature](./index.md)

# ReaderViewFeature

`class ReaderViewFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md)`, `[`UserInteractionHandler`](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/readerview/src/main/java/mozilla/components/feature/readerview/ReaderViewFeature.kt#L50)

Feature implementation that provides a reader view for the selected
session, based on a web extension.

### Types

| Name | Summary |
|---|---|
| [ColorScheme](-color-scheme/index.md) | `enum class ColorScheme` |
| [FontType](-font-type/index.md) | `enum class FontType` |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ReaderViewFeature(context: <ERROR CLASS>, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, controlsView: `[`ReaderViewControlsView`](../../mozilla.components.feature.readerview.view/-reader-view-controls-view/index.md)`, onReaderViewStatusChange: `[`onReaderViewStatusChange`](../on-reader-view-status-change.md)` = { _, _ -> Unit })`<br>Feature implementation that provides a reader view for the selected session, based on a web extension. |

### Functions

| Name | Summary |
|---|---|
| [hideControls](hide-controls.md) | `fun hideControls(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Hides the reader view appearance controls. |
| [hideReaderView](hide-reader-view.md) | `fun hideReaderView(session: `[`TabSessionState`](../../mozilla.components.browser.state.state/-tab-session-state/index.md)`? = store.state.selectedTab): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Hides the reader view UI. |
| [onBackPressed](on-back-pressed.md) | `fun onBackPressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Called when this [UserInteractionHandler](../../mozilla.components.support.base.feature/-user-interaction-handler/index.md) gets the option to handle the user pressing the back key. |
| [showControls](show-controls.md) | `fun showControls(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Shows the reader view appearance controls. |
| [showReaderView](show-reader-view.md) | `fun showReaderView(session: `[`TabSessionState`](../../mozilla.components.browser.state.state/-tab-session-state/index.md)`? = store.state.selectedTab): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Shows the reader view UI. |
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Inherited Functions

| Name | Summary |
|---|---|
| [onHomePressed](../../mozilla.components.support.base.feature/-user-interaction-handler/on-home-pressed.md) | `open fun onHomePressed(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>In most cases, when the home button is pressed, we invoke this callback to inform the app that the user is going to leave the app. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
