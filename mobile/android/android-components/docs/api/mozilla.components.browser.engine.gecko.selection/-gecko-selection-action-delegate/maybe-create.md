[android-components](../../index.md) / [mozilla.components.browser.engine.gecko.selection](../index.md) / [GeckoSelectionActionDelegate](index.md) / [maybeCreate](./maybe-create.md)

# maybeCreate

`fun maybeCreate(context: <ERROR CLASS>, customDelegate: `[`SelectionActionDelegate`](../../mozilla.components.concept.engine.selection/-selection-action-delegate/index.md)`?): `[`GeckoSelectionActionDelegate`](index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/engine-gecko-beta/src/main/java/mozilla/components/browser/engine/gecko/selection/GeckoSelectionActionDelegate.kt#L30)

**Returns**
a [GeckoSelectionActionDelegate](index.md) if [customDelegate](maybe-create.md#mozilla.components.browser.engine.gecko.selection.GeckoSelectionActionDelegate.Companion$maybeCreate(, mozilla.components.concept.engine.selection.SelectionActionDelegate)/customDelegate) is non-null and [context](maybe-create.md#mozilla.components.browser.engine.gecko.selection.GeckoSelectionActionDelegate.Companion$maybeCreate(, mozilla.components.concept.engine.selection.SelectionActionDelegate)/context)
is an instance of [Activity](#). Otherwise, returns null.

