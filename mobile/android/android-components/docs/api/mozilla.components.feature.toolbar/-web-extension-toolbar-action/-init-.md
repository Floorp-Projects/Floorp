[android-components](../../index.md) / [mozilla.components.feature.toolbar](../index.md) / [WebExtensionToolbarAction](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`WebExtensionToolbarAction(imageDrawable: <ERROR CLASS>, contentDescription: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, enabled: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, badgeText: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, badgeTextColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, badgeBackgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, padding: `[`Padding`](../../mozilla.components.support.base.android/-padding/index.md)`? = null, listener: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`)`

An action button that represents an web extension item to be added to the toolbar.

### Parameters

`imageDrawable` - The drawable to be shown.

`contentDescription` - The content description to use.

`enabled` - Indicates whether this button is enabled or not.

`badgeText` - The text shown above of the [imageDrawable](#).

`badgeTextColor` - The color of the [badgeText](#).

`badgeBackgroundColor` - The background color of the badge.

`padding` - A optional custom padding.

`listener` - Callback that will be invoked whenever the button is pressed