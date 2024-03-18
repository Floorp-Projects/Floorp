/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner

/**
 * An interface for all entry points to feature components to implement in order to make them lifecycle aware.
 */
interface LifecycleAwareFeature : DefaultLifecycleObserver {

    /**
     * Method that is called after ON_START event occurred.
     */
    fun start()

    /**
     * Method that is called after ON_STOP event occurred.
     */
    fun stop()

    override fun onStart(owner: LifecycleOwner) {
        start()
    }

    override fun onStop(owner: LifecycleOwner) {
        stop()
    }
}
