/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleRegistry
import android.arch.lifecycle.LifecycleOwner

/**
 * A [LifecycleOwner] which is always in a [Lifecycle.State.STARTED] state.
 */
class TestLifeCycleOwner : LifecycleOwner {
    private val registry = LifecycleRegistry(this)

    init {
        registry.markState(android.arch.lifecycle.Lifecycle.State.STARTED)
    }

    override fun getLifecycle(): Lifecycle = registry
}