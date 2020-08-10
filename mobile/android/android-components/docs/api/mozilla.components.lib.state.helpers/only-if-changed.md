[android-components](../index.md) / [mozilla.components.lib.state.helpers](index.md) / [onlyIfChanged](./only-if-changed.md)

# onlyIfChanged

`fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, T> onlyIfChanged(onMainThread: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)` = false, map: (`[`S`](only-if-changed.md#S)`) -> `[`T`](only-if-changed.md#T)`?, then: (`[`S`](only-if-changed.md#S)`, `[`T`](only-if-changed.md#T)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`, scope: CoroutineScope = GlobalScope): `[`Observer`](../mozilla.components.lib.state/-observer.md)`<`[`S`](only-if-changed.md#S)`>` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/helpers/Helpers.kt#L23)

Creates an [Observer](../mozilla.components.lib.state/-observer.md) that will map the received [State](../mozilla.components.lib.state/-state.md) to [T](only-if-changed.md#T) (using [map](only-if-changed.md#mozilla.components.lib.state.helpers$onlyIfChanged(kotlin.Boolean, kotlin.Function1((mozilla.components.lib.state.helpers.onlyIfChanged.S, mozilla.components.lib.state.helpers.onlyIfChanged.T)), kotlin.Function2((mozilla.components.lib.state.helpers.onlyIfChanged.S, mozilla.components.lib.state.helpers.onlyIfChanged.T, kotlin.Unit)), kotlinx.coroutines.CoroutineScope)/map)) and will invoke the callback
[then](only-if-changed.md#mozilla.components.lib.state.helpers$onlyIfChanged(kotlin.Boolean, kotlin.Function1((mozilla.components.lib.state.helpers.onlyIfChanged.S, mozilla.components.lib.state.helpers.onlyIfChanged.T)), kotlin.Function2((mozilla.components.lib.state.helpers.onlyIfChanged.S, mozilla.components.lib.state.helpers.onlyIfChanged.T, kotlin.Unit)), kotlinx.coroutines.CoroutineScope)/then) only if the value has changed from the last mapped value.

### Parameters

`onMainThread` - Whether or not the [then](only-if-changed.md#mozilla.components.lib.state.helpers$onlyIfChanged(kotlin.Boolean, kotlin.Function1((mozilla.components.lib.state.helpers.onlyIfChanged.S, mozilla.components.lib.state.helpers.onlyIfChanged.T)), kotlin.Function2((mozilla.components.lib.state.helpers.onlyIfChanged.S, mozilla.components.lib.state.helpers.onlyIfChanged.T, kotlin.Unit)), kotlinx.coroutines.CoroutineScope)/then) function should be executed on the main thread.

`map` - A function that maps [State](../mozilla.components.lib.state/-state.md) to the value we want to observe for changes.

`then` - Function that gets invoked whenever the mapped value changed.