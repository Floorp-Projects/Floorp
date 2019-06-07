[android-components](../../index.md) / [mozilla.components.ui.doorhanger](../index.md) / [Doorhanger](./index.md)

# Doorhanger

`class ~~Doorhanger~~` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/ui/doorhanger/src/main/java/mozilla/components/ui/doorhanger/Doorhanger.kt#L22)
**Deprecated:** The ui-doorhanger component is getting removed.

A [Doorhanger](./index.md) is a floating heads-up popup that can be anchored to a view. They are presented to notify the user
of something that is important.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Doorhanger(view: `[`View`](https://developer.android.com/reference/android/view/View.html)`, onDismiss: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = null)`<br>A [Doorhanger](./index.md) is a floating heads-up popup that can be anchored to a view. They are presented to notify the user of something that is important. |

### Functions

| Name | Summary |
|---|---|
| [dismiss](dismiss.md) | `fun dismiss(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Dismiss this doorhanger if it is currently showing. |
| [show](show.md) | `fun show(anchor: `[`View`](https://developer.android.com/reference/android/view/View.html)`): `[`PopupWindow`](https://developer.android.com/reference/android/widget/PopupWindow.html)<br>Show this doorhanger and anchor it to the given [View](https://developer.android.com/reference/android/view/View.html). |
