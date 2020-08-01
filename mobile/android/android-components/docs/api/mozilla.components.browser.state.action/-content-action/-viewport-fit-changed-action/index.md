[android-components](../../../index.md) / [mozilla.components.browser.state.action](../../index.md) / [ContentAction](../index.md) / [ViewportFitChangedAction](./index.md)

# ViewportFitChangedAction

`data class ViewportFitChangedAction : `[`ContentAction`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/state/action/BrowserAction.kt#L271)

Updates the [layoutInDisplayCutoutMode](layout-in-display-cutout-mode.md) with the given [sessionId](session-id.md).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ViewportFitChangedAction(sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, layoutInDisplayCutoutMode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`)`<br>Updates the [layoutInDisplayCutoutMode](layout-in-display-cutout-mode.md) with the given [sessionId](session-id.md). |

### Properties

| Name | Summary |
|---|---|
| [layoutInDisplayCutoutMode](layout-in-display-cutout-mode.md) | `val layoutInDisplayCutoutMode: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>value of defined in https://developer.android.com/reference/android/view/WindowManager.LayoutParams#layoutInDisplayCutoutMode |
| [sessionId](session-id.md) | `val sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the ID of the session |
