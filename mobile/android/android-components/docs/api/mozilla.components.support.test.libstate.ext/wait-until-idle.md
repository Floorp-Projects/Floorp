[android-components](../index.md) / [mozilla.components.support.test.libstate.ext](index.md) / [waitUntilIdle](./wait-until-idle.md)

# waitUntilIdle

`fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../mozilla.components.lib.state/-action.md)`> `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`S`](wait-until-idle.md#S)`, `[`A`](wait-until-idle.md#A)`>.waitUntilIdle(): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test-libstate/src/main/java/mozilla/components/support/test/libstate/ext/Store.kt#L19)

Blocks and returns once all dispatched actions have been processed
i.e. the reducers have run and all observers have been notified of
state changes.

