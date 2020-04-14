[android-components](../../index.md) / [mozilla.components.concept.storage](../index.md) / [Login](./index.md)

# Login

`data class Login` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/storage/src/main/java/mozilla/components/concept/storage/LoginsStorage.kt#L131)

Represents a login that can be used by autofill APIs.

Note that much of this information can be partial (e.g., a user might save a password with a
blank username).

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Login(guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, formActionOrigin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, httpRealm: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, username: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, password: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, timesUsed: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)` = 0, timeCreated: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0L, timeLastUsed: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0L, timePasswordChanged: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)` = 0L, usernameField: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null, passwordField: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`<br>Represents a login that can be used by autofill APIs. |

### Properties

| Name | Summary |
|---|---|
| [formActionOrigin](form-action-origin.md) | `val formActionOrigin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The origin this login entry was submitted to. This only applies to form-based login entries. It's derived from the action attribute set on the form element. |
| [guid](guid.md) | `val guid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The unique identifier for this login entry. |
| [httpRealm](http-realm.md) | `val httpRealm: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>The HTTP realm this login entry was requested for. This only applies to non-form-based login entries. It's derived from the WWW-Authenticate header set in a HTTP 401 response, see RFC2617 for details. |
| [origin](origin.md) | `val origin: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The origin this login entry applies to. |
| [password](password.md) | `val password: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The password for this login entry. |
| [passwordField](password-field.md) | `val passwordField: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>HTML field associated with the [password](password.md). |
| [timeCreated](time-created.md) | `val timeCreated: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Time of creation in milliseconds from the unix epoch. |
| [timeLastUsed](time-last-used.md) | `val timeLastUsed: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Time of last use in milliseconds from the unix epoch. |
| [timePasswordChanged](time-password-changed.md) | `val timePasswordChanged: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Time of last password change in milliseconds from the unix epoch. |
| [timesUsed](times-used.md) | `val timesUsed: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)<br>Number of times this password has been used. |
| [username](username.md) | `val username: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>The username for this login entry. |
| [usernameField](username-field.md) | `val usernameField: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?`<br>HTML field associated with the [username](username.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
| [toServerPassword](../../mozilla.components.service.sync.logins/to-server-password.md) | `fun `[`Login`](./index.md)`.toServerPassword(): `[`ServerPassword`](../../mozilla.components.service.sync.logins/-server-password.md)<br>Converts an Android Components [Login](./index.md) to an Application Services [ServerPassword](../../mozilla.components.service.sync.logins/-server-password.md) |
