[android-components](../../index.md) / [mozilla.components.browser.toolbar.display](../index.md) / [SiteSecurityIcons](./index.md)

# SiteSecurityIcons

`data class SiteSecurityIcons` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/display/SiteSecurityIcons.kt#L25)

Specifies icons to display in the toolbar representing the security of the current website.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `SiteSecurityIcons(insecure: <ERROR CLASS>?, secure: <ERROR CLASS>?)`<br>Specifies icons to display in the toolbar representing the security of the current website. |

### Properties

| Name | Summary |
|---|---|
| [insecure](insecure.md) | `val insecure: <ERROR CLASS>?`<br>Icon to display for HTTP sites. |
| [secure](secure.md) | `val secure: <ERROR CLASS>?`<br>Icon to display for HTTPS sites. |

### Functions

| Name | Summary |
|---|---|
| [withColorFilter](with-color-filter.md) | `fun withColorFilter(insecureColorFilter: <ERROR CLASS>, secureColorFilter: <ERROR CLASS>): `[`SiteSecurityIcons`](./index.md)<br>Returns an instance of [SiteSecurityIcons](./index.md) with a color filter applied to each icon.`fun withColorFilter(insecureColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, secureColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`SiteSecurityIcons`](./index.md)<br>Returns an instance of [SiteSecurityIcons](./index.md) with a color tint applied to each icon. |

### Companion Object Functions

| Name | Summary |
|---|---|
| [getDefaultSecurityIcons](get-default-security-icons.md) | `fun getDefaultSecurityIcons(context: <ERROR CLASS>, color: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`SiteSecurityIcons`](./index.md) |
