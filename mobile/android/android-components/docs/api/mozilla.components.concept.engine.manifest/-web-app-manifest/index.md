[android-components](../../index.md) / [mozilla.components.concept.engine.manifest](../index.md) / [WebAppManifest](./index.md)

# WebAppManifest

`data class WebAppManifest` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifest.kt#L49)

The web app manifest provides information about an application (such as its name, author, icon, and description).

Web app manifests are part of a collection of web technologies called progressive web apps, which are websites
that can be installed to a device’s homescreen without an app store, along with other capabilities like working
offline and receiving push notifications.

https://developer.mozilla.org/en-US/docs/Web/Manifest
https://www.w3.org/TR/appmanifest/
https://developers.google.com/web/fundamentals/web-app-manifest/

### Types

| Name | Summary |
|---|---|
| [DisplayMode](-display-mode/index.md) | `enum class DisplayMode`<br>Defines the developers’ preferred display mode for the website. |
| [ExternalApplicationResource](-external-application-resource/index.md) | `data class ExternalApplicationResource`<br>An external native application that is related to the web app. |
| [Icon](-icon/index.md) | `data class Icon`<br>An image file that can serve as application icon. |
| [Orientation](-orientation/index.md) | `enum class Orientation`<br>Defines the default orientation for all the website's top level browsing contexts. |
| [TextDirection](-text-direction/index.md) | `enum class TextDirection`<br>Specifies the primary text direction for the name, short_name, and description members. Together with the lang member, it helps the correct display of right-to-left languages. |

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebAppManifest(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, shortName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, display: `[`DisplayMode`](-display-mode/index.md)` = DisplayMode.BROWSER, backgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, description: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, icons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Icon`](-icon/index.md)`> = emptyList(), dir: `[`TextDirection`](-text-direction/index.md)` = TextDirection.AUTO, lang: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, orientation: `[`Orientation`](-orientation/index.md)` = Orientation.ANY, scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, themeColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`? = null, relatedApplications: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ExternalApplicationResource`](-external-application-resource/index.md)`> = emptyList(), preferRelatedApplications: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false)`<br>The web app manifest provides information about an application (such as its name, author, icon, and description). |

### Properties

| Name | Summary |
|---|---|
| [backgroundColor](background-color.md) | `val backgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>Defines the expected “background color” for the website. This value repeats what is already available in the site’s CSS, but can be used by browsers to draw the background color of a shortcut when the manifest is available before the stylesheet has loaded. This creates a smooth transition between launching the web application and loading the site's content. |
| [description](description.md) | `val description: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Provides a general description of what the pinned website does. |
| [dir](dir.md) | `val dir: `[`TextDirection`](-text-direction/index.md)<br>Specifies the primary text direction for the name, short_name, and description members. Together with the lang member, it helps the correct display of right-to-left languages. |
| [display](display.md) | `val display: `[`DisplayMode`](-display-mode/index.md)<br>Defines the developers’ preferred display mode for the website. |
| [icons](icons.md) | `val icons: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Icon`](-icon/index.md)`>`<br>Specifies a list of image files that can serve as application icons, depending on context. For example, they can be used to represent the web application amongst a list of other applications, or to integrate the web application with an OS's task switcher and/or system preferences. |
| [lang](lang.md) | `val lang: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Specifies the primary language for the values in the name and short_name members. This value is a string containing a single language tag (e.g. en-US). |
| [name](name.md) | `val name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Provides a human-readable name for the site when displayed to the user. For example, among a list of other applications or as a label for an icon. |
| [orientation](orientation.md) | `val orientation: `[`Orientation`](-orientation/index.md)<br>Defines the default orientation for all the website's top level browsing contexts. |
| [preferRelatedApplications](prefer-related-applications.md) | `val preferRelatedApplications: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>If true, related applications should be preferred over the web app. |
| [relatedApplications](related-applications.md) | `val relatedApplications: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`ExternalApplicationResource`](-external-application-resource/index.md)`>`<br>List of native applications related to the web app. |
| [scope](scope.md) | `val scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Defines the navigation scope of this website's context. This restricts what web pages can be viewed while the manifest is applied. If the user navigates outside the scope, it returns to a normal web page inside a browser tab/window. |
| [shortName](short-name.md) | `val shortName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>Provides a short human-readable name for the application. This is intended for when there is insufficient space to display the full name of the web application, like device homescreens. |
| [startUrl](start-url.md) | `val startUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The URL that loads when a user launches the application (e.g. when added to home screen), typically the index. Note that this has to be a relative URL, relative to the manifest url. |
| [themeColor](theme-color.md) | `val themeColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?`<br>Defines the default theme color for an application. This sometimes affects how the OS displays the site (e.g., on Android's task switcher, the theme color surrounds the site). |

### Extension Functions

| Name | Summary |
|---|---|
| [getTrustedScope](../../mozilla.components.feature.pwa.ext/get-trusted-scope.md) | `fun `[`WebAppManifest`](./index.md)`.getTrustedScope(): <ERROR CLASS>?`<br>Returns the scope of the manifest as a [Uri](#) for use with [mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature](../../mozilla.components.feature.pwa.feature/-web-app-hide-toolbar-feature/index.md). |
| [toCustomTabConfig](../../mozilla.components.feature.pwa.ext/to-custom-tab-config.md) | `fun `[`WebAppManifest`](./index.md)`.toCustomTabConfig(): `[`CustomTabConfig`](../../mozilla.components.browser.state.state/-custom-tab-config/index.md)<br>Creates a [CustomTabConfig](../../mozilla.components.browser.state.state/-custom-tab-config/index.md) that styles a custom tab toolbar to match the manifest theme. |
| [toIconRequest](../../mozilla.components.browser.icons.extension/to-icon-request.md) | `fun `[`WebAppManifest`](./index.md)`.toIconRequest(): `[`IconRequest`](../../mozilla.components.browser.icons/-icon-request/index.md)<br>Creates an [IconRequest](../../mozilla.components.browser.icons/-icon-request/index.md) for retrieving the icon specified in the manifest. |
| [toTaskDescription](../../mozilla.components.feature.pwa.ext/to-task-description.md) | `fun `[`WebAppManifest`](./index.md)`.toTaskDescription(icon: <ERROR CLASS>?): <ERROR CLASS>`<br>Creates a [TaskDescription](#) for the activity manager based on the manifest. |
