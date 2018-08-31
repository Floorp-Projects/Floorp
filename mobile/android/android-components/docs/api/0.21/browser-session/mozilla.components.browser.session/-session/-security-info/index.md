---
title: Session.SecurityInfo - 
---

[mozilla.components.browser.session](../../index.html) / [Session](../index.html) / [SecurityInfo](./index.html)

# SecurityInfo

`data class SecurityInfo`

A value type holding security information for a Session.

### Constructors

| [&lt;init&gt;](-init-.html) | `SecurityInfo(secure: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "", issuer: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "")`<br>A value type holding security information for a Session. |

### Properties

| [host](host.html) | `val host: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>domain for which the SSL certificate was issued. |
| [issuer](issuer.html) | `val issuer: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>name of the certificate authority who issued the SSL certificate. |
| [secure](secure.html) | `val secure: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>true if the session is currently pointed to a URL with a valid SSL certificate, otherwise false. |

