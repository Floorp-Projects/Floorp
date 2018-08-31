---
title: EditToolbar.<init> - 
---

[mozilla.components.browser.toolbar.edit](../index.html) / [EditToolbar](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`EditToolbar(context: Context, toolbar: `[`BrowserToolbar`](../../mozilla.components.browser.toolbar/-browser-toolbar/index.html)`)`

Sub-component of the browser toolbar responsible for allowing the user to edit the URL.

Structure:
+---------------------------------+---------+------+
|                 url             | actions | exit |
+---------------------------------+---------+------+

* url: Editable URL of the currently displayed website
* actions: Optional action icons injected by other components (e.g. barcode scanner)
* exit: Button that switches back to display mode.
