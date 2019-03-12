[android-components](../../index.md) / [mozilla.components.browser.session.manifest](../index.md) / [WebAppManifest](index.md) / [backgroundColor](./background-color.md)

# backgroundColor

`val backgroundColor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/manifest/WebAppManifest.kt#L49)

Defines the expected “background color” for the website. This value repeats what is
already available in the site’s CSS, but can be used by browsers to draw the background color of a shortcut when
the manifest is available before the stylesheet has loaded. This creates a smooth transition between launching the
web application and loading the site's content.

### Property

`backgroundColor` - Defines the expected “background color” for the website. This value repeats what is
already available in the site’s CSS, but can be used by browsers to draw the background color of a shortcut when
the manifest is available before the stylesheet has loaded. This creates a smooth transition between launching the
web application and loading the site's content.