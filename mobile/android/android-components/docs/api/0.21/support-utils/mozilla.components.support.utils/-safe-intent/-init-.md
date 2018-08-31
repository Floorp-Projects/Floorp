---
title: SafeIntent.<init> - 
---

[mozilla.components.support.utils](../index.html) / [SafeIntent](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`SafeIntent(unsafe: Intent)`

External applications can pass values into Intents that can cause us to crash: in defense,
we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385
for more.

