[android-components](../../index.md) / [mozilla.components.service.sync.logins](../index.md) / [AsyncLoginsStorageAdapter](index.md) / [ensureLocked](./ensure-locked.md)

# ensureLocked

`open fun ensureLocked(): Deferred<`[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L301)

Overrides [AsyncLoginsStorage.ensureLocked](../-async-logins-storage/ensure-locked.md)

Equivalent to `lock()`, but does not throw in the case that
the database is already unlocked. Never rejects.

