[android-components](../../index.md) / [mozilla.components.feature.session.bundling](../index.md) / [SessionBundleStorage](index.md) / [autoClose](./auto-close.md)

# autoClose

`@Synchronized fun autoClose(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/session-bundling/src/main/java/mozilla/components/feature/session/bundling/SessionBundleStorage.kt#L199)

Automatically clears the current bundle and starts a new bundle if the lifetime has expired while the app was in
the background.

