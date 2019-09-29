[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [SyncEngine](./index.md)

# SyncEngine

`sealed class SyncEngine` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Config.kt#L54)

Describes possible sync engines that device can support.

### Types

| Name | Summary |
|---|---|
| [Bookmarks](-bookmarks.md) | `object Bookmarks : `[`SyncEngine`](./index.md)<br>A bookmarks engine. |
| [History](-history.md) | `object History : `[`SyncEngine`](./index.md)<br>A history engine. |
| [Other](-other/index.md) | `data class Other : `[`SyncEngine`](./index.md)<br>An engine that's none of the above, described by [name](-other/name.md). |
| [Passwords](-passwords.md) | `object Passwords : `[`SyncEngine`](./index.md)<br>A 'logins/passwords' engine. |

### Properties

| Name | Summary |
|---|---|
| [nativeName](native-name.md) | `val nativeName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Internally, Rust SyncManager represents engines as strings. Forward-compatibility with new engines is one of the reasons for this. E.g. during any sync, an engine may appear that we do not know about. At the public API level, we expose a concrete [SyncEngine](./index.md) type to allow for more robust integrations. We do not expose "unknown" engines via our public API, but do handle them internally (by persisting their enabled/disabled status). |

### Inheritors

| Name | Summary |
|---|---|
| [Bookmarks](-bookmarks.md) | `object Bookmarks : `[`SyncEngine`](./index.md)<br>A bookmarks engine. |
| [History](-history.md) | `object History : `[`SyncEngine`](./index.md)<br>A history engine. |
| [Other](-other/index.md) | `data class Other : `[`SyncEngine`](./index.md)<br>An engine that's none of the above, described by [name](-other/name.md). |
| [Passwords](-passwords.md) | `object Passwords : `[`SyncEngine`](./index.md)<br>A 'logins/passwords' engine. |
