[android-components](../../index.md) / [mozilla.components.concept.sync](../index.md) / [SyncAuthInfo](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`SyncAuthInfo(kid: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fxaAccessToken: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, fxaAccessTokenExpiresAt: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, syncKey: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, tokenServerUrl: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`)`

A Firefox Sync friendly auth object which can be obtained from [OAuthAccount](../-o-auth-account/index.md).

Why is there a Firefox Sync-shaped authentication object at the concept level, you ask?
Mainly because this is what the [SyncableStore](../-syncable-store/index.md) consumes in order to actually perform
synchronization, which is in turn implemented by `places`-backed storage layer.
If this class lived in `services-firefox-accounts`, we'd end up with an ugly dependency situation
between services and storage components.

Turns out that building a generic description of an authentication/synchronization layer is not
quite the way to go when you only have a single, legacy implementation.

However, this may actually improve once we retire the tokenserver from the architecture.
We could also consider a heavier use of generics, as well.

