/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.LifecycleOwner
import android.arch.lifecycle.OnLifecycleEvent
import android.view.View
import mozilla.components.browser.session.store.BrowserStore
import mozilla.components.browser.session.store.Observer

/**
 * Registers an [Observer] function that will be invoked whenever the state changes. The [BrowserStore.Subscription]
 * will be bound to the passed in [LifecycleOwner]. Once the [Lifecycle] state changes to DESTROYED the [Observer] will
 * be unregistered automatically.
 */
fun BrowserStore.observe(
    owner: LifecycleOwner,
    receiveInitialState: Boolean = true,
    observer: Observer
): BrowserStore.Subscription {
    return observe(receiveInitialState, observer).apply {
        binding = LifecycleBoundObserver(owner, this)
    }
}

/**
 * Registers an [Observer] function that will be invoked whenever the state changes. The [BrowserStore.Subscription]
 * will be bound to the passed in [View]. Once the [View] gets detached the [Observer] will be unregistered
 * automatically.
 */
fun BrowserStore.observe(
    view: View,
    observer: Observer,
    receiveInitialState: Boolean = true
): BrowserStore.Subscription {
    return observe(receiveInitialState, observer).apply {
        binding = ViewBoundObserver(view, this)
    }
}

/**
 * GenericLifecycleObserver implementation to bind an observer to a Lifecycle.
 */
private class LifecycleBoundObserver(
    private val owner: LifecycleOwner,
    private val subscription: BrowserStore.Subscription
) : LifecycleObserver, BrowserStore.Subscription.Binding {
    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    override fun unbind() {
        subscription.unsubscribe()

        owner.lifecycle.removeObserver(this)
    }
}

/**
 * View.OnAttachStateChangeListener implementation to bind an observer to a View.
 */
private class ViewBoundObserver(
    private val view: View,
    private val subscription: BrowserStore.Subscription
) : View.OnAttachStateChangeListener, BrowserStore.Subscription.Binding {
    override fun onViewAttachedToWindow(v: View?) = Unit

    override fun onViewDetachedFromWindow(view: View) {
        subscription.unsubscribe()
    }

    override fun unbind() {
        view.removeOnAttachStateChangeListener(this)
    }
}
