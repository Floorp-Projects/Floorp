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
| [pause](pause.md) | `fun pause(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Pauses the [Subscription](./index.md). The [Observer](../../-observer.md) will not get notified when the state changes until [resume](resume.md) is called. |
| [resume](resume.md) | `fun resume(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Resumes the [Subscription](./index.md). The [Observer](../../-observer.md) will get notified for every state change. Additionally it will get invoked immediately with the latest state. |
| [unsubscribe](unsubscribe.md) | `fun unsubscribe(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Unsubscribe from the [Store](../index.md). |
