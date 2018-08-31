---
title: ObserverRegistry.register - 
---

[mozilla.components.support.base.observer](../index.html) / [ObserverRegistry](index.html) / [register](./register.html)

# register

`fun register(observer: `[`T`](index.html#T)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Overrides [Observable.register](../-observable/register.html)

Registers an observer to get notified about changes.

`fun register(observer: `[`T`](index.html#T)`, owner: LifecycleOwner): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Overrides [Observable.register](../-observable/register.html)

Registers an observer to get notified about changes.

The observer will automatically unsubscribe if the lifecycle of the provided LifecycleOwner
becomes DESTROYED.

`fun register(observer: `[`T`](index.html#T)`, view: View): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Overrides [Observable.register](../-observable/register.html)

Registers an observer to get notified about changes.

The observer will automatically unsubscribe if the provided view gets detached.

