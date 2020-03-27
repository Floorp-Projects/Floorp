[android-components](../../index.md) / [mozilla.components.feature.tab.collections](../index.md) / [Tab](index.md) / [restore](./restore.md)

# restore

`abstract fun restore(context: <ERROR CLASS>, engine: `[`Engine`](../../mozilla.components.concept.engine/-engine/index.md)`, restoreSessionId: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false): `[`Item`](../../mozilla.components.browser.session/-session-manager/-snapshot/-item/index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tab-collections/src/main/java/mozilla/components/feature/tab/collections/Tab.kt#L38)

Restores a single tab from this collection and returns a matching [Session](../../mozilla.components.browser.session/-session/index.md).

### Parameters

`restoreSessionId` - If true the original [Session.id](../../mozilla.components.browser.session/-session/id.md) of [Session](../../mozilla.components.browser.session/-session/index.md)s will be restored. Otherwise a new ID
will be generated. An app may prefer to use a new ID if it expects sessions to get restored multiple times -
otherwise breaking the promise of a unique ID per [Session](../../mozilla.components.browser.session/-session/index.md).