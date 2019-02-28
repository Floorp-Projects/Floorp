[android-components](../../index.md) / [mozilla.components.concept.engine.webextension](../index.md) / [WebExtension](./index.md)

# WebExtension

`data class WebExtension` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/webextension/WebExtension.kt#L15)

Represents a browser extension based on the WebExtension API:
https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `WebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`<br>Represents a browser extension based on the WebExtension API: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions |

### Properties

| Name | Summary |
|---|---|
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the unique ID of this extension. |
| [url](url.md) | `val url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the url pointing to a resources path for locating the extension within the APK file e.g. resource://android/assets/extensions/my_web_ext. |
