---
title: SafeBundle.<init> - 
---

[mozilla.components.support.utils](../index.html) / [SafeBundle](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`SafeBundle(bundle: Bundle)`

See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily
experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles,
and we need to handle those defensively too.

See bug 1090385 for more.

