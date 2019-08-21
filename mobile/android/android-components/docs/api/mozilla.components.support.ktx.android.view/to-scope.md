[android-components](../index.md) / [mozilla.components.support.ktx.android.view](index.md) / [toScope](./to-scope.md)

# toScope

`@MainThread fun <ERROR CLASS>.toScope(): CoroutineScope` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/android/view/View.kt#L93)

Creates a [CoroutineScope](#) that is active as long as this [View](#) is attached. Once this [View](#)
gets detached this [CoroutineScope](#) gets cancelled automatically.

By default coroutines dispatched on the created [CoroutineScope](#) run on the main dispatcher.

Note: This scope gets only cancelled if the [View](#) gets detached. In cases where the [View](#) never
gets attached this may create a scope that never gets cancelled!

