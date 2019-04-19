[android-components](../../../index.md) / [mozilla.components.browser.session.store](../../index.md) / [BrowserStore](../index.md) / [Subscription](./index.md)

# Subscription

`class Subscription` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/state/src/main/java/mozilla/components/browser/session/store/BrowserStore.kt#L93)

A [Subscription](./index.md) is returned whenever an observer is registered via the [observe](../observe.md) method. Calling [unsubscribe](unsubscribe.md)
on the [Subscription](./index.md) will unregister the observer.

### Types

| Name | Summary |
|---|---|
| [Binding](-binding/index.md) | `interface Binding` |

### Properties

| Name | Summary |
|---|---|
| [binding](binding.md) | `var binding: `[`Binding`](-binding/index.md)`?` |

### Functions

| Name | Summary |
|---|---|
| [unsubscribe](unsubscribe.md) | `fun unsubscribe(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |
