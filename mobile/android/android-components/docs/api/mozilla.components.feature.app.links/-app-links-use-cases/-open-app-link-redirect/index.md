[android-components](../../../index.md) / [mozilla.components.feature.app.links](../../index.md) / [AppLinksUseCases](../index.md) / [OpenAppLinkRedirect](./index.md)

# OpenAppLinkRedirect

`inner class OpenAppLinkRedirect` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/AppLinksUseCases.kt#L183)

Open an external app with the redirect created by the [GetAppLinkRedirect](../-get-app-link-redirect/index.md).

This does not do any additional UI other than the chooser that Android may provide the user.

### Functions

| Name | Summary |
|---|---|
| [invoke](invoke.md) | `operator fun invoke(appIntent: <ERROR CLASS>?, launchInNewTask: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = true, failedToLaunchAction: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)` = {}): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
