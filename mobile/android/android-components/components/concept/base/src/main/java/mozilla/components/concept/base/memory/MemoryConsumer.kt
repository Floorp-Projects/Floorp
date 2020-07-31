/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.base.memory

import android.content.ComponentCallbacks2

/**
 * Interface for components that can seize large amounts of memory and support trimming in low
 * memory situations.
 *
 * Also see [ComponentCallbacks2].
 */
interface MemoryConsumer {
    /**
     * Notifies this component that it should try to release memory.
     *
     * Should be called from a [ComponentCallbacks2] providing the level passed to
     * [ComponentCallbacks2.onTrimMemory].
     *
     * @param level The context of the trim, giving a hint of the amount of
     * trimming the application may like to perform. See constants in [ComponentCallbacks2].
     */
    fun onTrimMemory(level: Int)
}
