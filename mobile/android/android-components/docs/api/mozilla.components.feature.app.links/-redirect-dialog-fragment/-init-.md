[android-components](../../index.md) / [mozilla.components.feature.app.links](../index.md) / [RedirectDialogFragment](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`RedirectDialogFragment()`

This is a general representation of a dialog meant to be used in collaboration with [AppLinksFeature](../-app-links-feature/index.md)
to show a dialog before an external link is opened.
If [SimpleRedirectDialogFragment](../-simple-redirect-dialog-fragment/index.md) is not flexible enough for your use case you should inherit for this class.
Be mindful to call [onConfirmRedirect](on-confirm-redirect.md) when you want to open the linked app.

