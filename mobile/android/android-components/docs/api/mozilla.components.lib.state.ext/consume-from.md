[android-components](../index.md) / [mozilla.components.lib.state.ext](index.md) / [consumeFrom](./consume-from.md)

# consumeFrom

`@ExperimentalCoroutinesApi fun <S : `[`State`](../mozilla.components.lib.state/-state.md)`, A : `[`Action`](../mozilla.components.lib.state/-action.md)`> <ERROR CLASS>.consumeFrom(store: `[`Store`](../mozilla.components.lib.state/-store/index.md)`<`[`S`](consume-from.md#S)`, `[`A`](consume-from.md#A)`>, owner: LifecycleOwner, block: (`[`S`](consume-from.md#S)`) -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/ext/View.kt#L28)

Helper extension method for consuming [State](../mozilla.components.lib.state/-state.md) from a [Store](../mozilla.components.lib.state/-store/index.md) sequentially in order scoped to the
lifetime of the [View](#). The [block](consume-from.md#mozilla.components.lib.state.ext$consumeFrom(, mozilla.components.lib.state.Store((mozilla.components.lib.state.ext.consumeFrom.S, mozilla.components.lib.state.ext.consumeFrom.A)), androidx.lifecycle.LifecycleOwner, kotlin.Function1((mozilla.components.lib.state.ext.consumeFrom.S, kotlin.Unit)))/block) function will get invoked for every [State](../mozilla.components.lib.state/-state.md) update.

This helper will automatically stop observing the [Store](../mozilla.components.lib.state/-store/index.md) once the [View](#) gets detached. The
provided [LifecycleOwner](#) is used to determine when observing should be stopped or resumed.

Inside a [Fragment](#) prefer to use [Fragment.consumeFrom](androidx.fragment.app.-fragment/consume-from.md).

