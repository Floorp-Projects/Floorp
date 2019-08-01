[android-components](../index.md) / [mozilla.components.service.sync.logins](index.md) / [InvalidRecordException](./-invalid-record-exception.md)

# InvalidRecordException

`typealias InvalidRecordException = InvalidRecordException` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/sync-logins/src/main/java/mozilla/components/service/sync/logins/AsyncLoginsStorage.kt#L86)

This is thrown on attempts to insert or update a record so that it
is no longer valid, where "invalid" is defined as such:

* A record with a blank `password` is invalid.
* A record with a blank `hostname` is invalid.
* A record that doesn't have a `formSubmitURL` nor a `httpRealm` is invalid.
* A record that has both a `formSubmitURL` and a `httpRealm` is invalid.
