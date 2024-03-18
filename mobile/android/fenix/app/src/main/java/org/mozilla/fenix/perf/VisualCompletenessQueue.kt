/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.view.View
import androidx.core.view.doOnPreDraw
import mozilla.components.support.utils.RunWhenReadyQueue
import java.lang.ref.WeakReference

/**
 * class for all functionality related to Visual completeness queue
 */
class VisualCompletenessQueue(val queue: RunWhenReadyQueue) {
    @Suppress("MagicNumber")
    val delay = 5000L

    /**
     *
     * @param containerWeakReference a weak reference to the root view of a view hierarchy. Weak
     * reference is to avoid memory leak.
     */
    fun attachViewToRunVisualCompletenessQueueLater(containerWeakReference: WeakReference<View>) {
        containerWeakReference.get()?.doOnPreDraw {
            // This delay is temporary. We are delaying 5 seconds until the performance
            // team can locate the real point of visual completeness.
            it.postDelayed(
                {
                    queue.ready()
                },
                delay,
            )
        }
    }
}
