[android-components](../../../index.md) / [mozilla.components.feature.addons](../../index.md) / [Addon](../index.md) / [InstalledState](./index.md)

# InstalledState

`data class InstalledState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/Addon.kt#L94)

Returns a list of id resources per each item on the [Addon.permissions](../permissions.md) list.
Holds the state of the installed web extension of this add-on.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `InstalledState(id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, version: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, optionsPageUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, supported: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true)`<br>Returns a list of id resources per each item on the [Addon.permissions](../permissions.md) list. Holds the state of the installed web extension of this add-on. |

### Properties

| Name | Summary |
|---|---|
| [enabled](enabled.md) | `val enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicates if this [Addon](../index.md) is enabled to interact with web content or not, defaults to false. |
| [id](id.md) | `val id: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The ID of the installed web extension. |
| [optionsPageUrl](options-page-url.md) | `val optionsPageUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>the URL of the page displaying the options page (options_ui in the extension's manifest). |
| [supported](supported.md) | `val supported: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Indicates if this [Addon](../index.md) is supported by the browser or not, defaults to true. |
| [version](version.md) | `val version: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The installed version. |
