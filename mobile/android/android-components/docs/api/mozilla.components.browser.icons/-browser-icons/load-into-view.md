[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [BrowserIcons](index.md) / [loadIntoView](./load-into-view.md)

# loadIntoView

`fun loadIntoView(view: <ERROR CLASS>, request: `[`IconRequest`](../-icon-request/index.md)`, placeholder: <ERROR CLASS>? = null, error: <ERROR CLASS>? = null): Job` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/BrowserIcons.kt#L161)

Loads an icon asynchronously using [BrowserIcons](index.md) and then displays it in the [ImageView](#).
If the view is detached from the window before loading is completed, then loading is cancelled.

### Parameters

`view` - [ImageView](#) to load icon into.

`request` - Load icon for this given [IconRequest](../-icon-request/index.md).

`placeholder` - [Drawable](#) to display while icon is loading.

`error` - [Drawable](#) to display if loading fails.