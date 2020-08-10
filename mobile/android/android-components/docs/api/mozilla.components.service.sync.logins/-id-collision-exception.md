[android-components](../index.md) / [mozilla.components.service.sync.logins](index.md) / [IdCollisionException](./-id-collision-exception.md)

# IdCollisionException

`typealias IdCollisionException = IdCollisionException` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/SyncableLoginsStorage.kt#L81)

This is thrown if `add()` is given a record whose `id` is not blank, and
collides with a record already known to the storage instance.

You can avoid ever worrying about this error by always providing blank
`id` property when inserting new records.

