[android-components](../../index.md) / [mozilla.components.feature.tab.collections](../index.md) / [TabCollection](index.md) / [restoreSubset](./restore-subset.md)

# restoreSubset

`abstract fun restoreSubset(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, tabs: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Tab`](../-tab/index.md)`>): `[`Snapshot`](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tab-collections/src/main/java/mozilla/components/feature/tab/collections/TabCollection.kt#L38)

Restores a subset of the tabs in this collection and returns a matching [SessionManager.Snapshot](../../mozilla.components.browser.session/-session-manager/-snapshot/index.md).

