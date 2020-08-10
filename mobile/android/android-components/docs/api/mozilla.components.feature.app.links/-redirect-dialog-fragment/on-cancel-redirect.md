[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [RedirectDialogFragment](index.md) / [onCancelRedirect](./on-cancel-redirect.md)

# onCancelRedirect

`var onCancelRedirect: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/app-links/src/main/java/mozilla/components/feature/app/links/RedirectDialogFragment.kt#L30)

A callback to trigger when user dismisses the dialog.
For instance, a valid use case can be in confirmation dialog, after the negative button is clicked,
this callback must be called.

