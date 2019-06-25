[android-components](../../index.md) / [mozilla.components.lib.state](../index.md) / [Store](index.md) / [observeManually](./observe-manually.md)

# observeManually

`@CheckResult("observe") @Synchronized fun observeManually(observer: `[`Observer`](../-observer.md)`<`[`S`](index.md#S)`>): `[`Subscription`](-subscription/index.md)`<`[`S`](index.md#S)`, `[`A`](index.md#A)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt#L65)

Registers an [Observer](../-observer.md) function that will be invoked whenever the [State](../-state.md) changes.

Right after registering the [Observer](../-observer.md) will be invoked with the current [State](../-state.md).

It's the responsibility of the caller to keep track of the returned [Subscription](-subscription/index.md) and call
[Subscription.unsubscribe](-subscription/unsubscribe.md) to stop observing and avoid potentially leaking memory by keeping an unused [Observer](../-observer.md)
registered. It's is recommend to use one of the `observe` extension methods that unsubscribe automatically.

**Return**
A [Subscription](-subscription/index.md) object that can be used to unsubscribe from further state changes.

