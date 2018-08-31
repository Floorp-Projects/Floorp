---
title: SafeIntent - 
---

[mozilla.components.support.utils](../index.html) / [SafeIntent](./index.html)

# SafeIntent

`class SafeIntent`

External applications can pass values into Intents that can cause us to crash: in defense,
we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385
for more.

### Constructors

| [&lt;init&gt;](-init-.html) | `SafeIntent(unsafe: Intent)`<br>External applications can pass values into Intents that can cause us to crash: in defense, we wrap [Intent](#) and catch the exceptions they may force us to throw. See bug 1090385 for more. |

### Properties

| [action](action.html) | `val action: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [categories](categories.html) | `val categories: `[`Set`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-set/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?` |
| [data](data.html) | `val data: Uri?` |
| [dataString](data-string.html) | `val dataString: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [extras](extras.html) | `val extras: Bundle?` |
| [flags](flags.html) | `val flags: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [isLauncherIntent](is-launcher-intent.html) | `val isLauncherIntent: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [unsafe](unsafe.html) | `val unsafe: Intent` |

### Functions

| [getBooleanExtra](get-boolean-extra.html) | `fun getBooleanExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, defaultValue: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getBundleExtra](get-bundle-extra.html) | `fun getBundleExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`SafeBundle`](../-safe-bundle/index.html)`?` |
| [getCharSequenceExtra](get-char-sequence-extra.html) | `fun getCharSequenceExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`CharSequence`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-char-sequence/index.html)`?` |
| [getIntExtra](get-int-extra.html) | `fun getIntExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, defaultValue: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html) |
| [getParcelableArrayListExtra](get-parcelable-array-list-extra.html) | `fun <T : Parcelable> getParcelableArrayListExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`ArrayList`](http://docs.oracle.com/javase/6/docs/api/java/util/ArrayList.html)`<`[`T`](get-parcelable-array-list-extra.html#T)`>?` |
| [getParcelableExtra](get-parcelable-extra.html) | `fun <T : Parcelable> getParcelableExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](get-parcelable-extra.html#T)`?` |
| [getStringArrayListExtra](get-string-array-list-extra.html) | `fun getStringArrayListExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`ArrayList`](http://docs.oracle.com/javase/6/docs/api/java/util/ArrayList.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?` |
| [getStringExtra](get-string-extra.html) | `fun getStringExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` |
| [hasExtra](has-extra.html) | `fun hasExtra(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |

