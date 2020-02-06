[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [InFlightMigrationState](./index.md)

# InFlightMigrationState

`enum class InFlightMigrationState` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/sync/src/main/java/mozilla/components/concept/sync/OAuthAccount.kt#L49)

Represents a specific type of an "in-flight" migration state that could result from intermittent
issues during [OAuthAccount.migrateFromSessionTokenAsync](../-o-auth-account/migrate-from-session-token-async.md) or [OAuthAccount.copyFromSessionTokenAsync](../-o-auth-account/copy-from-session-token-async.md).

### Enum Values

| Name | Summary |
|---|---|
| [NONE](-n-o-n-e.md) | No in-flight migration. |
| [COPY_SESSION_TOKEN](-c-o-p-y_-s-e-s-s-i-o-n_-t-o-k-e-n.md) | "Copy" in-flight migration present. Can retry migration via [OAuthAccount.retryMigrateFromSessionTokenAsync](../-o-auth-account/retry-migrate-from-session-token-async.md). |
| [REUSE_SESSION_TOKEN](-r-e-u-s-e_-s-e-s-s-i-o-n_-t-o-k-e-n.md) | "Reuse" in-flight migration present. Can retry migration via [OAuthAccount.retryMigrateFromSessionTokenAsync](../-o-auth-account/retry-migrate-from-session-token-async.md). |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
