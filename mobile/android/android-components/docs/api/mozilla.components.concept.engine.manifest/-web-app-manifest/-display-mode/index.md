[android-components](../../../index.md) / [mozilla.components.concept.engine.manifest](../../index.md) / [WebAppManifest](../index.md) / [DisplayMode](./index.md)

# DisplayMode

`enum class DisplayMode` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifest.kt#L69)

Defines the developers’ preferred display mode for the website.

### Enum Values

| Name | Summary |
|---|---|
| [FULLSCREEN](-f-u-l-l-s-c-r-e-e-n.md) | All of the available display area is used and no user agent chrome is shown. |
| [STANDALONE](-s-t-a-n-d-a-l-o-n-e.md) | The application will look and feel like a standalone application. This can include the application having a different window, its own icon in the application launcher, etc. In this mode, the user agent will exclude UI elements for controlling navigation, but can include other UI elements such as a status bar. |
| [MINIMAL_UI](-m-i-n-i-m-a-l_-u-i.md) | The application will look and feel like a standalone application, but will have a minimal set of UI elements for controlling navigation. The elements will vary by browser. |
| [BROWSER](-b-r-o-w-s-e-r.md) | The application opens in a conventional browser tab or new window, depending on the browser and platform. This is the default. |
