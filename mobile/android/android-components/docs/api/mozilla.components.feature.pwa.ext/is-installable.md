[android-components](../index.md) / [mozilla.components.feature.pwa.ext](index.md) / [isInstallable](./is-installable.md)

# isInstallable

`fun `[`Session`](../mozilla.components.browser.session/-session/index.md)`.isInstallable(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ext/Session.kt#L21)

Checks if the current session represents an installable web app.

Websites are installable if:

* The site is served over HTTPS
* The site has a valid manifest with a name or short_name
* The icons array in the manifest contains an icon of at least 192x192
