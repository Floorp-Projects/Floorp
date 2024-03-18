# [Android Components](../../../README.md) > Feature > App-Links

A session component to support opening non-browser apps and `intent://` style URLs.

## Usage

From a `BrowserFragment`:
```kotlin
// Start listening to the intercepted and offer to open app banners
AppLinksFeature(
    context = context,
    sessionManager = sessionManager,
    sessionId = customSessionId,
    fragmentManager = fragmentManager
)
```

From elsewhere in the app:
```kotlin
val redirect = AppLinksUseCases.appLinkRedirect.invoke(redirect)

if (redirect.isExternalApp()) {
    AppLinkUseCases.openAppLink(redirect)
}
```

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/)
 ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-app-links:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
