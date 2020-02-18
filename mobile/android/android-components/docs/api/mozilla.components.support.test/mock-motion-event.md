[android-components](../index.md) / [mozilla.components.support.test](index.md) / [mockMotionEvent](./mock-motion-event.md)

# mockMotionEvent

`fun mockMotionEvent(action: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, downTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis(), eventTime: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = System.currentTimeMillis(), x: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)` = 0f, y: `[`Float`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-float/index.html)` = 0f, metaState: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test/src/main/java/mozilla/components/support/test/Mock.kt#L39)

Creates a custom [MotionEvent](#) for testing. As of SDK 28 [MotionEvent](#)s can't be mocked anymore and need to be created
through [MotionEvent.obtain](#).

