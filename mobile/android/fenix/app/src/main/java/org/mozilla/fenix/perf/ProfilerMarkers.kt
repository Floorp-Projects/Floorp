/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.view.View
import androidx.core.view.doOnPreDraw
import mozilla.components.concept.base.profiler.Profiler

/**
 * A container for functions for when adding a profiler marker is less readable
 * (e.g. multiple lines, more advanced logic).
 */
object ProfilerMarkers {

    fun homeActivityOnStart(rootContainer: View, profiler: Profiler?) {
        rootContainer.doOnPreDraw {
            profiler?.addMarker("onPreDraw", "expected first frame via HomeActivity.onStart")
        }
    }
}
