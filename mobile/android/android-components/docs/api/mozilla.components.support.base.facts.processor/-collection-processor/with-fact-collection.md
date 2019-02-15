[android-components](../../index.md) / [mozilla.components.support.base.facts.processor](../index.md) / [CollectionProcessor](index.md) / [withFactCollection](./with-fact-collection.md)

# withFactCollection

`fun withFactCollection(block: (`[`List`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-list/index.html)`<`[`Fact`](../../mozilla.components.support.base.facts/-fact/index.md)`>) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/facts/processor/CollectionProcessor.kt#L42)

Helper for creating a [CollectionProcessor](index.md), registering it and clearing the processors again.

Use in tests like:

```
CollectionProcessor.withFactCollection { facts ->
  // During execution of this block the "facts" list will be updated automatically to contain
  // all facts that were emitted while executing this block.
  // After this block has completed all registered processors will be cleared.
}
```

