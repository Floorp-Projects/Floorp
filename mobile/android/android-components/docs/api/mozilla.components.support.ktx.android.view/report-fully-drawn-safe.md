[android-components](../index.md) / [mozilla.components.support.ktx.android.view](index.md) / [reportFullyDrawnSafe](./report-fully-drawn-safe.md)

# reportFullyDrawnSafe

`fun <ERROR CLASS>.reportFullyDrawnSafe(errorLogger: `[`Logger`](../mozilla.components.support.base.log.logger/-logger/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/view/Activity.kt#L43)

Calls [Activity.reportFullyDrawn](#) while also preventing crashes under some circumstances.

### Parameters

`errorLogger` - the logger to be used if errors are logged.