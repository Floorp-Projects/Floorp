/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils.view

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.Lifecycle.State
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry

/**
 * Provides a [LifecycleOwner] on a given [View] for features that function on lifecycle events.
 *
 * When the [View] is attached to the window, observers will receive the [Lifecycle.Event.ON_RESUME] event.
 * When the [View] is detached to the window, observers will receive the [Lifecycle.Event.ON_STOP] event.
 *
 * @param view The [View] that will be observed.
 */
class LifecycleViewProvider(view: View) : LifecycleOwner {
    private val registry = LifecycleRegistry(this)

    init {
        registry.currentState = State.INITIALIZED

        view.addOnAttachStateChangeListener(ViewBinding(registry))
    }

    override val lifecycle: Lifecycle
        get() = registry
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal class ViewBinding(
    private val registry: LifecycleRegistry,
) : View.OnAttachStateChangeListener {
    override fun onViewAttachedToWindow(v: View) {
        registry.currentState = State.RESUMED
    }

    override fun onViewDetachedFromWindow(v: View) {
        registry.currentState = State.DESTROYED
    }
}
