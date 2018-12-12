/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.support.base.feature

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent

/**
 * An interface for all entry points to feature components to implement in order to make them lifecycle aware.
 */
interface LifecycleAwareFeature : LifecycleObserver {
    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun start()

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun stop()
}
