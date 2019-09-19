[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [WebAppHideToolbarFeature](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebAppHideToolbarFeature(sessionManager: `[`SessionManager`](../../mozilla.components.browser.session/-session-manager/index.md)`, toolbar: <ERROR CLASS>, sessionId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, trustedScopes: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<<ERROR CLASS>>, onToolbarVisibilityChange: (visible: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {})`

Hides a custom tab toolbar for Progressive Web Apps and Trusted Web Activities.

When the [Session](../../mozilla.components.browser.session/-session/index.md) is inside a trusted scope, the toolbar will be hidden.
Once the [Session](../../mozilla.components.browser.session/-session/index.md) navigates to another scope, the toolbar will be revealed.

### Parameters

`toolbar` - Toolbar to show or hide.

`sessionId` - ID of the custom tab session.

`trustedScopes` - Scopes to hide the toolbar at.
Scopes correspond to [WebAppManifest.scope](../../mozilla.components.concept.engine.manifest/-web-app-manifest/scope.md). They can be a path (PWA) or just an origin (TWA).

`onToolbarVisibilityChange` - Called when the toolbar is changed to be visible or hidden.