[android-components](../../index.md) / [mozilla.components.browser.toolbar.edit](../index.md) / [EditToolbar](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`EditToolbar(context: `[`Context`](https://developer.android.com/reference/android/content/Context.html)`, toolbar: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.md)`)`

Sub-component of the browser toolbar responsible for allowing the user to edit the URL.

Structure:
+---------------------------------+---------+------+
|                 url             | actions | exit |
+---------------------------------+---------+------+

* url: Editable URL of the currently displayed website
* actions: Optional action icons injected by other components (e.g. barcode scanner)
* exit: Button that switches back to display mode.
