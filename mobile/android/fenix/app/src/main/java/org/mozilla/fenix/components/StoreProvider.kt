/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.annotation.MainThread
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.ViewModelStoreOwner
import androidx.lifecycle.get
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CoroutineScope
import mozilla.components.lib.state.Store

/**
 * Generic ViewModel to wrap a State object for state restoration.
 *
 * @param createStore Function that creates a [Store] instance that receives [CoroutineScope] param
 * that's tied to the lifecycle of the [StoreProvider] i.e it will cancel when
 * [StoreProvider.onCleared] is called.
 */
class StoreProvider<T : Store<*, *>>(
    createStore: (CoroutineScope) -> T,
) : ViewModel() {

    @VisibleForTesting
    internal val store: T = createStore(viewModelScope)

    companion object {
        /**
         * Returns an existing [Store] instance or creates a new one scoped to a
         * [ViewModelStoreOwner].
         * @see [ViewModelProvider.get].
         */
        fun <T : Store<*, *>> get(owner: ViewModelStoreOwner, createStore: (CoroutineScope) -> T): T {
            val factory = StoreProviderFactory(createStore)
            val viewModel: StoreProvider<T> = ViewModelProvider(owner, factory).get()
            return viewModel.store
        }
    }
}

/**
 * ViewModel factory to create [StoreProvider] instances.
 *
 * @param createStore Callback to create a new [Store], used when the [ViewModel] is first created.
 */
@VisibleForTesting
class StoreProviderFactory<T : Store<*, *>>(
    private val createStore: (CoroutineScope) -> T,
) : ViewModelProvider.Factory {

    @Suppress("UNCHECKED_CAST")
    override fun <VM : ViewModel> create(modelClass: Class<VM>): VM {
        return StoreProvider(createStore) as VM
    }
}

/**
 * Helper function for lazy creation of a [Store] instance scoped to a [ViewModelStoreOwner].
 *
 * @param createStore Function that creates a [Store] instance that receives [CoroutineScope] param
 * that's tied to the lifecycle of the [StoreProvider] i.e it will cancel when
 * [StoreProvider.onCleared] is called.
 *
 * Example:
 * ```
 * val store by lazy { scope ->
 *   MyStore(
 *     middleware = listOf(
 *       MyMiddleware(
 *         settings = requireComponents.settings,
 *         ...
 *         scope = scope,
 *       ),
 *     )
 *   )
 * }
 */
@MainThread
fun <T : Store<*, *>> ViewModelStoreOwner.lazyStore(
    createStore: (CoroutineScope) -> T,
): Lazy<T> {
    return lazy(mode = LazyThreadSafetyMode.NONE) {
        StoreProvider.get(this, createStore)
    }
}
