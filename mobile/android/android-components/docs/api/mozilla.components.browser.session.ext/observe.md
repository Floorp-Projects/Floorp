[android-components](../index.md) / [mozilla.components.browser.session.ext](index.md) / [observe](./observe.md)

# observe

`fun `[`BrowserStore`](../mozilla.components.browser.session.store/-browser-store/index.md)`.observe(owner: LifecycleOwner, receiveInitialState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, observer: `[`Observer`](../mozilla.components.browser.session.store/-observer.md)`): `[`Subscription`](../mozilla.components.browser.session.store/-browser-store/-subscription/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/ext/StoreExtensions.kt#L20)

Registers an [Observer](../mozilla.components.browser.session.store/-observer.md) function that will be invoked whenever the state changes. The [BrowserStore.Subscription](../mozilla.components.browser.session.store/-browser-store/-subscription/index.md)
will be bound to the passed in [LifecycleOwner](#). Once the [Lifecycle](#) state changes to DESTROYED the [Observer](../mozilla.components.browser.session.store/-observer.md) will
be unregistered automatically.

`fun `[`BrowserStore`](../mozilla.components.browser.session.store/-browser-store/index.md)`.observe(view: `[`View`](https://developer.android.com/reference/android/view/View.html)`, observer: `[`Observer`](../mozilla.components.browser.session.store/-observer.md)`, receiveInitialState: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true): `[`Subscription`](../mozilla.components.browser.session.store/-browser-store/-subscription/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/ext/StoreExtensions.kt#L35)

Registers an [Observer](../mozilla.components.browser.session.store/-observer.md) function that will be invoked whenever the state changes. The [BrowserStore.Subscription](../mozilla.components.browser.session.store/-browser-store/-subscription/index.md)
will be bound to the passed in [View](https://developer.android.com/reference/android/view/View.html). Once the [View](https://developer.android.com/reference/android/view/View.html) gets detached the [Observer](../mozilla.components.browser.session.store/-observer.md) will be unregistered
automatically.

