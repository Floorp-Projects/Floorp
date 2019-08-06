[android-components](../../index.md) / [mozilla.components.feature.customtabs.verify](../index.md) / [OriginVerifier](index.md) / [verifyOrigin](./verify-origin.md)

# verifyOrigin

`suspend fun verifyOrigin(origin: <ERROR CLASS>): <ERROR CLASS>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/customtabs/src/main/java/mozilla/components/feature/customtabs/verify/OriginVerifier.kt#L54)

Verify the claimed origin for the cached package name asynchronously. This will end up
making a network request for non-cached origins with a HTTP [Client](../../mozilla.components.concept.fetch/-client/index.md).

### Parameters

`origin` - The postMessage origin the application is claiming to have. Can't be null.