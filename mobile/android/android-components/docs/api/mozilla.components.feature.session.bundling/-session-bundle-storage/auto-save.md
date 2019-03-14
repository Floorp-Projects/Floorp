[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](index.md) / [autoSave](./auto-save.md)

# autoSave

`@CheckResult fun autoSave(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, interval: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = AutoSave.DEFAULT_INTERVAL_MILLISECONDS, unit: `[`TimeUnit`](https://developer.android.com/reference/java/util/concurrent/TimeUnit.html)` = TimeUnit.MILLISECONDS): `[`AutoSave`](../../mozilla.components.browser.session.storage/-auto-save/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundleStorage.kt#L186)

Starts configuring automatic saving of the state.

