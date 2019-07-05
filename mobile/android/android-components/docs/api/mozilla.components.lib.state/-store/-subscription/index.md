[android-components](../../../index.md) / [mozilla.components.lib.state](../../index.md) / [Store](../index.md) / [Subscription](./index.md)

# Subscription

`class Subscription<S : `[`State`](../../-state.md)`, A : `[`Action`](../../-action.md)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt#L126)

A [Subscription](./index.md) is returned whenever an observer is registered via the [observeManually](../observe-manually.md) method. Calling
[unsubscribe](unsubscribe.md) on the [Subscription](./index.md) will unregister the observer.

### Types

| Name | Summary |
|---|---|
| [Binding](-binding/index.md) | `interface Binding` |

### Functions

| Name | Summary |
|---|---|
| [unsubscribe](unsubscribe.md) | `fun unsubscribe(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
