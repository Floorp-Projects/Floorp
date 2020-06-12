[android-components](../../index.md) / [mozilla.components.feature.pwa.feature](../index.md) / [WebAppHideToolbarFeature](./index.md)

# WebAppHideToolbarFeature

`class WebAppHideToolbarFeature : `[`LifecycleAwareFeature`](../../mozilla.components.support.base.feature/-lifecycle-aware-feature/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/feature/WebAppHideToolbarFeature.kt#L49)

Hides a custom tab toolbar for Progressive Web Apps and Trusted Web Activities.

When the [Session](../../mozilla.components.browser.session/-session/index.md) is inside a trusted scope, the toolbar will be hidden.
Once the [Session](../../mozilla.components.browser.session/-session/index.md) navigates to another scope, the toolbar will be revealed.
The toolbar is also hidden in fullscreen mode or picture in picture mode.

In standard custom tabs, no scopes are trusted.
As a result the URL has no impact on toolbar visibility.

### Parameters

`store` - Reference to the browser store where tab state is located.

`customTabsStore` - Reference to the store that communicates with the custom tabs service.

`tabId` - ID of the tab session, or null if the selected session should be used.

`manifest` - Reference to the cached [WebAppManifest](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md) for the current PWA.
Null if this feature is not used in a PWA context.

`setToolbarVisibility` - Callback to show or hide the toolbar.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppHideToolbarFeature(store: `[`BrowserStore`](../../mozilla.components.browser.state.store/-browser-store/index.md)`, customTabsStore: `[`CustomTabsServiceStore`](../../mozilla.components.feature.customtabs.store/-custom-tabs-service-store/index.md)`, tabId: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, manifest: `[`WebAppManifest`](../../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md)`? = null, setToolbarVisibility: (`[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`<br>Hides a custom tab toolbar for Progressive Web Apps and Trusted Web Activities. |

### Functions

| Name | Summary |
|---|---|
| [start](start.md) | `fun start(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [stop](stop.md) | `fun stop(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
