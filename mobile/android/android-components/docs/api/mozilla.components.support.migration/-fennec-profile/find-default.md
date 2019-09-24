[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FennecProfile](index.md) / [findDefault](./find-default.md)

# findDefault

`fun findDefault(context: <ERROR CLASS>, mozillaDirectory: `[`File`](https://developer.android.com/reference/java/io/File.html)` = getMozillaDirectory(context), fileName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "profiles.ini"): `[`FennecProfile`](index.md)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/migration/src/main/java/mozilla/components/support/migration/FennecProfile.kt#L34)

Returns the default [FennecProfile](index.md) - the default profile used by Fennec or `null` if no
profile could be found.

If no explicit default profile could be found then this method will try to return a valid
profile that may be treated as the default profile:

* If there's only one profile then this profile will be returned.
* If there are multiple profiles with no default then the first profile will be returned.
