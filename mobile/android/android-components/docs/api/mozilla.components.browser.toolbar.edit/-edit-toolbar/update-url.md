[android-components](../../index.md) / [mozilla.components.browser.toolbar.edit](../index.md) / [EditToolbar](index.md) / [updateUrl](./update-url.md)

# updateUrl

`fun updateUrl(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/toolbar/src/main/java/mozilla/components/browser/toolbar/edit/EditToolbar.kt#L110)

Updates the URL. This should only be called if the toolbar is not in editing mode. Otherwise
this might override the URL the user is currently typing.

