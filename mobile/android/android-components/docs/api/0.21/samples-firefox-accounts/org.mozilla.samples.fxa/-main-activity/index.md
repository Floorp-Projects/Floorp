---
title: MainActivity - 
---

[org.mozilla.samples.fxa](../index.html) / [MainActivity](./index.html)

# MainActivity

`open class MainActivity : AppCompatActivity, `[`OnLoginCompleteListener`](../-login-fragment/-on-login-complete-listener/index.html)

### Constructors

| [&lt;init&gt;](-init-.html) | `MainActivity()` |

### Functions

| [onCreate](on-create.html) | `open fun onCreate(savedInstanceState: Bundle?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onDestroy](on-destroy.html) | `open fun onDestroy(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onLoginComplete](on-login-complete.html) | `open fun onLoginComplete(code: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, state: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fragment: `[`LoginFragment`](../-login-fragment/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
| [onNewIntent](on-new-intent.html) | `open fun onNewIntent(intent: Intent): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| [CLIENT_ID](-c-l-i-e-n-t_-i-d.html) | `const val CLIENT_ID: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [CONFIG_URL](-c-o-n-f-i-g_-u-r-l.html) | `const val CONFIG_URL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [FXA_STATE_KEY](-f-x-a_-s-t-a-t-e_-k-e-y.html) | `const val FXA_STATE_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [FXA_STATE_PREFS_KEY](-f-x-a_-s-t-a-t-e_-p-r-e-f-s_-k-e-y.html) | `const val FXA_STATE_PREFS_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [REDIRECT_URL](-r-e-d-i-r-e-c-t_-u-r-l.html) | `const val REDIRECT_URL: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

