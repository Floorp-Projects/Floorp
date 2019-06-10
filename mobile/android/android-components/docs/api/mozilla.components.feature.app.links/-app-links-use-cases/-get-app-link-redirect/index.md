[android-components](../../../index.md) / [mozilla.components.feature.app.links](../../index.md) / [AppLinksUseCases](../index.md) / [GetAppLinkRedirect](./index.md)

# GetAppLinkRedirect

`inner class GetAppLinkRedirect` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinksUseCases.kt#L68)

Parse a URL and check if it can be handled by an app elsewhere on the Android device.
If that app is not available, then a market place intent is also provided.

It will also provide a fallback.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `fun invoke(url: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`AppLinkRedirect`](../../-app-link-redirect/index.md) |
