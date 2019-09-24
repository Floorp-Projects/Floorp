[android-components](../../index.md) / [mozilla.components.feature.readerview.view](../index.md) / [ReaderViewControlsBar](index.md) / [setFontSize](./set-font-size.md)

# setFontSize

`fun setFontSize(size: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/readerview/src/main/java/mozilla/components/feature/readerview/view/ReaderViewControlsBar.kt#L68)

Overrides [ReaderViewControlsView.setFontSize](../-reader-view-controls-view/set-font-size.md)

Sets the font size of the current and future ReaderView sessions.

Note: The readerview.js implementation under the hood scales the entire page's contents and not just
the text size.

### Parameters

`size` - An integer value in the range [MIN_TEXT_SIZE](../-m-i-n_-t-e-x-t_-s-i-z-e.md) to [MAX_TEXT_SIZE](../-m-a-x_-t-e-x-t_-s-i-z-e.md).