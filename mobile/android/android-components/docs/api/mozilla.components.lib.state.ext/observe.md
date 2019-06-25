[android-components](../index.md) / [mozilla.components.lib.state.ext](index.md) / [observe](./observe.md)

# observe

`fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../mozilla.components.lib.state/-action.md)`> `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`S`](observe.md#S)`, `[`A`](observe.md#A)`>.observe(owner: LifecycleOwner, observer: `[`Observer`](../mozilla.components.lib.state/-observer.md)`<`[`S`](observe.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/ext/StoreExtensions.kt#L25)

Registers an [Observer](../mozilla.components.lib.state/-observer.md) function that will be invoked whenever the state changes. The [Store.Subscription](../mozilla.components.lib.state/-store/-subscription/index.md)
will be bound to the passed in [LifecycleOwner](#). Once the [Lifecycle](#) state changes to DESTROYED the [Observer](../mozilla.components.lib.state/-observer.md) will
be unregistered automatically.

Right after registering the [Observer](../mozilla.components.lib.state/-observer.md) will be invoked with the current [State](../mozilla.components.lib.state/-state.md).

`fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../mozilla.components.lib.state/-action.md)`> `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`S`](observe.md#S)`, `[`A`](observe.md#A)`>.observe(view: <ERROR CLASS>, observer: `[`Observer`](../mozilla.components.lib.state/-observer.md)`<`[`S`](observe.md#S)`>): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/ext/StoreExtensions.kt#L48)

Registers an [Observer](../mozilla.components.lib.state/-observer.md) function that will be invoked whenever the state changes. The [Store.Subscription](../mozilla.components.lib.state/-store/-subscription/index.md)
will be bound to the passed in [View](#). Once the [View](#) gets detached the [Observer](../mozilla.components.lib.state/-observer.md) will be unregistered
automatically.

Right after registering the [Observer](../mozilla.components.lib.state/-observer.md) will be invoked with the current [State](../mozilla.components.lib.state/-state.md).

