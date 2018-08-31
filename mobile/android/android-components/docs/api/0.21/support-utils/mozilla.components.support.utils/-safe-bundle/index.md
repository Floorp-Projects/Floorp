---
title: SafeBundle - 
---

[mozilla.components.support.utils](../index.html) / [SafeBundle](./index.html)

# SafeBundle

`class SafeBundle`

See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily
experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles,
and we need to handle those defensively too.

See bug 1090385 for more.

### Constructors

| [&lt;init&gt;](-init-.html) | `SafeBundle(bundle: Bundle)`<br>See SafeIntent for more background: applications can put garbage values into Bundles. This is primarily experienced when there's garbage in the Intent's Bundle. However that Bundle can contain further bundles, and we need to handle those defensively too. |

### Functions

| [getParcelable](get-parcelable.html) | `fun <T : Parcelable> getParcelable(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](get-parcelable.html#T)`?` |
| [getString](get-string.html) | `fun getString(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |

