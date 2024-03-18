/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.base.profiler.Profiler
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getAndIncrementNoOverflow
import java.lang.reflect.Modifier.PRIVATE
import java.util.concurrent.atomic.AtomicInteger

private val classPrefixList = arrayOf(
    "android.widget.",
    "android.webkit.",
    "android.app.",
)

/**
 * Counts the number of inflations fenix does. This class behaves only as an inflation counter since
 * it takes the `inflater` that is given by the base system. This is done in order not to change
 * the behavior of the app since all we want to do is count the inflations done.
 */
open class PerformanceInflater(
    inflater: LayoutInflater,
    context: Context,
) : LayoutInflater(
    inflater,
    context,
) {

    private val profiler: Profiler? = context.components.core.engine.profiler

    override fun cloneInContext(newContext: Context?): LayoutInflater {
        return PerformanceInflater(this, newContext!!)
    }

    override fun inflate(resource: Int, root: ViewGroup?, attachToRoot: Boolean): View {
        InflationCounter.inflationCount.getAndIncrementNoOverflow()

        val profilerStartTime = profiler?.getProfilerTime()
        val layout = super.inflate(resource, root, attachToRoot)

        // I'm not sure how expensive fetching a resource name is so only do it if the profiler is active.
        if (profiler?.isProfilerActive() == true) {
            val layoutName = context.resources.getResourceEntryName(resource)
            profiler.addMarker("LayoutInflater.inflate", profilerStartTime, layoutName)
        }
        return layout
    }

    /**
     * This code was taken from the PhoneLayoutInflater.java located in the android source code
     * (Similarly, AsyncLayoutInflater implements it the exact same way too which can be found in the
     * Android Framework). This piece of code was taken from the other inflaters implemented by Android
     * since we do not want to change the inflater behavior except to count the number of inflations
     * that our app is doing for performance purposes. Looking at the `super.OnCreateView(name, attrs)`,
     * it hardcodes the prefix as "android.view." this means that a xml element such as
     * ImageButton will crash the app using android.view.ImageButton. This method only works with
     * XML tag that contains no prefix. This means that views such as androidx.recyclerview... will not
     * work with this method.
     */
    @Suppress("EmptyCatchBlock")
    @Throws(ClassNotFoundException::class)
    override fun onCreateView(name: String?, attrs: AttributeSet?): View? {
        for (prefix in classPrefixList) {
            try {
                val view = createView(name, prefix, attrs)
                if (view != null) {
                    return view
                }
            } catch (e: ClassNotFoundException) {
                // We want the super class to inflate if ever the view can't be inflated here
            }
        }
        return super.onCreateView(name, attrs)
    }
}

@VisibleForTesting(otherwise = PRIVATE)
object InflationCounter {
    val inflationCount = AtomicInteger(0)
}
