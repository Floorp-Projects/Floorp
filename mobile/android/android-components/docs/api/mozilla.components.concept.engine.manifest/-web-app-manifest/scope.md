[android-components](../../index.md) / [mozilla.components.concept.engine.manifest](../index.md) / [WebAppManifest](index.md) / [scope](./scope.md)

# scope

`val scope: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifest.kt#L60)

Defines the navigation scope of this website's context. This restricts what web pages can be viewed
while the manifest is applied. If the user navigates outside the scope, it returns to a normal web page inside a
browser tab/window.

### Property

`scope` - Defines the navigation scope of this website's context. This restricts what web pages can be viewed
while the manifest is applied. If the user navigates outside the scope, it returns to a normal web page inside a
browser tab/window.