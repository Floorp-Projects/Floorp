[android-components](../../index.md) / [mozilla.components.support.utils](../index.md) / [SafeIntent](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SafeIntent(unsafe: <ERROR CLASS>)`

External applications can pass values into Intents that can cause us to crash: in defense,
we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385
for more.

