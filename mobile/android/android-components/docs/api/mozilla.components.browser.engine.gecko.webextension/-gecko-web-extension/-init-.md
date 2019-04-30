[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.webextension](../index.md) / [GeckoWebExtension](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`GeckoWebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, nativeExtension: `[`WebExtension`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html)` = GeckoNativeWebExtension(url, id))`
`GeckoWebExtension(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, allowContentMessaging: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, nativeExtension: `[`WebExtension`](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html)` = GeckoNativeWebExtension(
        url,
        id,
        createWebExtensionFlags(allowContentMessaging)
    ))`

Gecko-based implementation of [WebExtension](../../mozilla.components.concept.engine.webextension/-web-extension/index.md), wrapping the native web
extension object provided by GeckoView.

