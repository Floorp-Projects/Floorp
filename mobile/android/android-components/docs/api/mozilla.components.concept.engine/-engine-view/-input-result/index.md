[android-components](../../../index.md) / [mozilla.components.concept.engine](../../index.md) / [EngineView](../index.md) / [InputResult](./index.md)

# InputResult

`enum class InputResult` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/EngineView.kt#L135)

Enumeration of all possible ways user's [android.view.MotionEvent](#) was handled.

**See Also**

[INPUT_RESULT_UNHANDLED](-i-n-p-u-t_-r-e-s-u-l-t_-u-n-h-a-n-d-l-e-d.md)

[INPUT_RESULT_HANDLED](-i-n-p-u-t_-r-e-s-u-l-t_-h-a-n-d-l-e-d.md)

[INPUT_RESULT_HANDLED_CONTENT](-i-n-p-u-t_-r-e-s-u-l-t_-h-a-n-d-l-e-d_-c-o-n-t-e-n-t.md)

### Enum Values

| Name | Summary |
|---|---|
| [INPUT_RESULT_UNHANDLED](-i-n-p-u-t_-r-e-s-u-l-t_-u-n-h-a-n-d-l-e-d.md) | Last [android.view.MotionEvent](#) was not handled by neither us nor the webpage. |
| [INPUT_RESULT_HANDLED](-i-n-p-u-t_-r-e-s-u-l-t_-h-a-n-d-l-e-d.md) | We handled the last [android.view.MotionEvent](#). |
| [INPUT_RESULT_HANDLED_CONTENT](-i-n-p-u-t_-r-e-s-u-l-t_-h-a-n-d-l-e-d_-c-o-n-t-e-n-t.md) | Webpage handled the last [android.view.MotionEvent](#). (through it's own touch event listeners) |

### Properties

| Name | Summary |
|---|---|
| [value](value.md) | `val value: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
