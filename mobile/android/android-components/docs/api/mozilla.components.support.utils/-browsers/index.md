[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [Browsers](./index.md)

# Browsers

`class Browsers` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/Browsers.kt#L25)

Helpful tools for dealing with other browsers on this device.

```
// Collect information about all installed browsers:
val browsers = Browsers.all(context)

// Collect information about installed browsers (and apps) that can handle a specific URL:
val browsers = Browsers.forUrl(context, url)`
```

### Types

| Name | Summary |
|---|---|
| [KnownBrowser](-known-browser/index.md) | `enum class KnownBrowser`<br>Enum of known browsers and their package names. |

### Properties

| Name | Summary |
|---|---|
| [defaultBrowser](default-browser.md) | `val defaultBrowser: <ERROR CLASS>?`<br>The [ActivityInfo](#) of the default browser of the user (or null if none could be found). |
| [firefoxBrandedBrowser](firefox-branded-browser.md) | `val firefoxBrandedBrowser: <ERROR CLASS>?`<br>The [ActivityInfo](#) of the installed Firefox browser (or null if none could be found). |
| [hasFirefoxBrandedBrowserInstalled](has-firefox-branded-browser-installed.md) | `val hasFirefoxBrandedBrowserInstalled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is there a Firefox browser installed on this device? |
| [hasMultipleThirdPartyBrowsers](has-multiple-third-party-browsers.md) | `val hasMultipleThirdPartyBrowsers: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Does this device have multiple third-party browser installed? |
| [hasThirdPartyDefaultBrowser](has-third-party-default-browser.md) | `val hasThirdPartyDefaultBrowser: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Does this device have a default browser that is not Firefox (release) or **this** app calling the method. |
| [installedBrowsers](installed-browsers.md) | `val installedBrowsers: `[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<<ERROR CLASS>>`<br>List of [ActivityInfo](#) of all known installed browsers. |
| [isDefaultBrowser](is-default-browser.md) | `val isDefaultBrowser: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is **this** application the default browser? |
| [isFirefoxDefaultBrowser](is-firefox-default-browser.md) | `val isFirefoxDefaultBrowser: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Is Firefox (Release, Beta, Nightly) the default browser of the user? |

### Functions

| Name | Summary |
|---|---|
| [isInstalled](is-installed.md) | `fun isInstalled(browser: `[`KnownBrowser`](-known-browser/index.md)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Does this device have [browser](is-installed.md#mozilla.components.support.utils.Browsers$isInstalled(mozilla.components.support.utils.Browsers.KnownBrowser)/browser) installed? |

### Companion Object Functions

| Name | Summary |
|---|---|
| [all](all.md) | `fun all(context: <ERROR CLASS>): `[`Browsers`](./index.md)<br>Collect information about all installed browsers and return a [Browsers](./index.md) object containing that data. |
| [forUrl](for-url.md) | `fun forUrl(context: <ERROR CLASS>, url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Browsers`](./index.md)<br>Collect information about all installed browsers that can handle the specified URL and return a [Browsers](./index.md) object containing that data. |
