[android-components](../index.md) / [mozilla.components.browser.tabstray.ext](index.md) / [doOnTabsUpdated](./do-on-tabs-updated.md)

# doOnTabsUpdated

`inline fun `[`TabsAdapter`](../mozilla.components.browser.tabstray/-tabs-adapter/index.md)`.doOnTabsUpdated(crossinline action: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/ext/TabsAdapter.kt#L16)

Performs the given action when the [TabsTray.Observer.onTabsUpdated](../mozilla.components.concept.tabstray/-tabs-tray/-observer/on-tabs-updated.md) is invoked.

The action will only be invoked once and then removed.

