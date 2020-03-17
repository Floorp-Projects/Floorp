[android-components](../../index.md) / [mozilla.components.service.fxa](../index.md) / [SyncEngine](index.md) / [nativeName](./native-name.md)

# nativeName

`val nativeName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/firefox-accounts/src/main/java/mozilla/components/service/fxa/Config.kt#L65)

Internally, Rust SyncManager represents engines as strings. Forward-compatibility
with new engines is one of the reasons for this. E.g. during any sync, an engine may appear that we
do not know about. At the public API level, we expose a concrete [SyncEngine](index.md) type to allow for more
robust integrations. We do not expose "unknown" engines via our public API, but do handle them
internally (by persisting their enabled/disabled status).

### Property

`nativeName` - Internally, Rust SyncManager represents engines as strings. Forward-compatibility
with new engines is one of the reasons for this. E.g. during any sync, an engine may appear that we
do not know about. At the public API level, we expose a concrete [SyncEngine](index.md) type to allow for more
robust integrations. We do not expose "unknown" engines via our public API, but do handle them
internally (by persisting their enabled/disabled status).