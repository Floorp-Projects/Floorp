[android-components](../index.md) / [mozilla.components.feature.downloads.ext](index.md) / [withResponse](./with-response.md)

# withResponse

`fun `[`Download`](../mozilla.components.browser.session/-download/index.md)`.withResponse(headers: `[`Headers`](../mozilla.components.concept.fetch/-headers/index.md)`, stream: `[`InputStream`](https://developer.android.com/reference/java/io/InputStream.html)`?): `[`Download`](../mozilla.components.browser.session/-download/index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/downloads/src/main/java/mozilla/components/feature/downloads/ext/Download.kt#L28)

Returns a copy of the download with some fields filled in based on values from a response.

### Parameters

`headers` - Headers from the response.

`stream` - Stream of the response body.