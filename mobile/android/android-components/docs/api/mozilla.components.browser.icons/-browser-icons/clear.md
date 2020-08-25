[android-components](../../index.md) / [mozilla.components.browser.icons](../index.md) / [BrowserIcons](index.md) / [clear](./clear.md)

# clear

`fun clear(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/icons/src/main/java/mozilla/components/browser/icons/BrowserIcons.kt#L241)

Clears all icons and metadata from disk and memory.

This will clear the default disk and memory cache that is used by the default configuration.
If custom [IconLoader](../../mozilla.components.browser.icons.loader/-icon-loader/index.md) and [IconProcessor](../../mozilla.components.browser.icons.processor/-icon-processor/index.md) instances with a custom storage are provided to
[BrowserIcons](index.md) then the calling app is responsible for clearing that data.

