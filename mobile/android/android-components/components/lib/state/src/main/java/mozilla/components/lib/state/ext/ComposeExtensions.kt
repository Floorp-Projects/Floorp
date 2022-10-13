/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.os.Parcelable
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.Saver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import androidx.compose.runtime.State as ComposeState

/**
 * Starts observing this [Store] and represents the mapped state (using [map]) via [ComposeState].
 *
 * Every time the mapped [Store] state changes, the returned [ComposeState] will be updated causing
 * recomposition of every [ComposeState.value] usage.
 *
 * The [Store] observer will automatically be removed when this composable disposes or the current
 * [LifecycleOwner] moves to the [Lifecycle.State.DESTROYED] state.
 */
@Composable
fun <S : State, A : Action, R> Store<S, A>.observeAsComposableState(map: (S) -> R): ComposeState<R?> {
    val lifecycleOwner = LocalLifecycleOwner.current
    val state = remember { mutableStateOf<R?>(map(state)) }

    DisposableEffect(this, lifecycleOwner) {
        val subscription = observe(lifecycleOwner) { browserState ->
            state.value = map(browserState)
        }
        onDispose { subscription?.unsubscribe() }
    }

    return state
}

/**
 * Starts observing this [Store] and represents the mapped state (using [map]) via [ComposeState].
 *
 * Everytime the [Store] state changes and the result of the [observe] function changes for this
 * state, the returned [ComposeState] will be updated causing recomposition of every
 * [ComposeState.value] usage.
 *
 * The [Store] observer will automatically be removed when this composable disposes or the current
 * [LifecycleOwner] moves to the [Lifecycle.State.DESTROYED] state.
 */
@Composable
fun <S : State, A : Action, O, R> Store<S, A>.observeAsComposableState(
    observe: (S) -> O,
    map: (S) -> R,
): ComposeState<R?> {
    val lifecycleOwner = LocalLifecycleOwner.current
    var lastValue = observe(state)
    val state = remember { mutableStateOf<R?>(map(state)) }

    DisposableEffect(this, lifecycleOwner) {
        val subscription = observe(lifecycleOwner) { browserState ->
            val newValue = observe(browserState)
            if (newValue != lastValue) {
                state.value = map(browserState)
                lastValue = newValue
            }
        }
        onDispose { subscription?.unsubscribe() }
    }

    return state
}

/**
 * Helper for creating a [Store] scoped to a `@Composable` and whose [State] gets saved and restored
 * on process recreation.
 */
@Composable
inline fun <reified S : State, A : Action> composableStore(
    crossinline save: (S) -> Parcelable = { state ->
        if (state is Parcelable) {
            state
        } else {
            throw NotImplementedError(
                "State of store does not implement Parcelable. Either implement Parcelable or pass " +
                    "custom save function to composableStore()",
            )
        }
    },
    crossinline restore: (Parcelable) -> S = { parcelable ->
        if (parcelable is S) {
            parcelable
        } else {
            throw NotImplementedError(
                "Restored parcelable is not of same class as state. Either the state needs to " +
                    "implement Parcelable or you need to provide a custom restore function to composableStore()",
            )
        }
    },
    crossinline init: (S?) -> Store<S, A>,
): Store<S, A> {
    return rememberSaveable(
        saver = Saver(
            save = { store -> save(store.state) },
            restore = { parcelable ->
                val state = restore(parcelable)
                init(state)
            },
        ),
        init = { init(null) },
    )
}
