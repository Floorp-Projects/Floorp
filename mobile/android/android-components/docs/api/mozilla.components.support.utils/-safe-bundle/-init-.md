[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [SafeBundle](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SafeBundle(unsafe: <ERROR CLASS>)`

See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily
experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles,
and we need to handle those defensively too.

See bug 1090385 for more.

