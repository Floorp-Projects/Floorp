[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [RedirectDialogFragment](index.md) / [onConfirmRedirect](./on-confirm-redirect.md)

# onConfirmRedirect

`var onConfirmRedirect: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/RedirectDialogFragment.kt#L23)

A callback to trigger a download, call it when you are ready to open the linked app. For instance,
a valid use case can be in confirmation dialog, after the positive button is clicked,
this callback must be called.

