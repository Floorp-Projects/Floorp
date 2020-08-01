[android-components](../../index.md) / [mozilla.components.browser.tabstray](../index.md) / [TabViewHolder](index.md) / [bind](./bind.md)

# bind

`abstract fun bind(tab: `[`Tab`](../../mozilla.components.concept.tabstray/-tab/index.md)`, isSelected: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`, styling: `[`TabsTrayStyling`](../-tabs-tray-styling/index.md)`, observable: `[`Observable`](../../mozilla.components.support.base.observer/-observable/index.md)`<`[`Observer`](../../mozilla.components.concept.tabstray/-tabs-tray/-observer/index.md)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/tabstray/src/main/java/mozilla/components/browser/tabstray/TabViewHolder.kt#L36)

Binds the ViewHolder to the `Tab`.

### Parameters

`tab` - the `Tab` used to bind the viewHolder.

`isSelected` - boolean to describe whether or not the `Tab` is selected.

`observable` - message bus to pass events to Observers of the TabsTray.