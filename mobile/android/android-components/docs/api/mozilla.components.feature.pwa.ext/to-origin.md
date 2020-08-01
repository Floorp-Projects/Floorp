[android-components](../index.md) / [mozilla.components.feature.pwa.ext](index.md) / [toOrigin](./to-origin.md)

# toOrigin

`fun <ERROR CLASS>.toOrigin(): <ERROR CLASS>?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/pwa/src/main/java/mozilla/components/feature/pwa/ext/Uri.kt#L17)

Returns just the origin of the [Uri](#).

The origin is a URL that contains only the scheme, host, and port.
https://html.spec.whatwg.org/multipage/origin.html#concept-origin

Null is returned if the URI was invalid (i.e.: `"/foo/bar".toUri()`)

