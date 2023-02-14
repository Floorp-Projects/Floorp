/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Context
import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.engine.Engine
import org.mozilla.fenix.browser.BaseBrowserFragment
import org.mozilla.fenix.home.HomeFragment

/**
 * Adds a profiler marker for each fragment lifecycle callbacks. The callbacks are called by the
 * super method (e.g. [Fragment.onCreate] so the markers occur sometime during the execution of
 * our implementation (e.g. [org.mozilla.fenix.home.HomeFragment.onCreate]) rather than at the
 * beginning or end of that method.
 */
@ExperimentalCoroutinesApi // reference to HomeFragment causes cascade.
@Suppress("TooManyFunctions") // it's the interface so we don't have a choice
class MarkersFragmentLifecycleCallbacks(
    private val engine: Engine,
) : FragmentManager.FragmentLifecycleCallbacks() {

    private fun shouldSkip(): Boolean {
        return engine.profiler?.isProfilerActive() != true
    }

    override fun onFragmentAttached(fm: FragmentManager, f: Fragment, context: Context) {
        if (shouldSkip()) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onAttach (via callbacks)")
    }

    override fun onFragmentPaused(fm: FragmentManager, f: Fragment) {
        if (shouldSkip()) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onPause (via callbacks)")
    }

    override fun onFragmentStopped(fm: FragmentManager, f: Fragment) {
        if (shouldSkip()) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onStop (via callbacks)")
    }

    override fun onFragmentDestroyed(fm: FragmentManager, f: Fragment) {
        if (shouldSkip()) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onDestroy (via callbacks)")
    }

    override fun onFragmentDetached(fm: FragmentManager, f: Fragment) {
        if (shouldSkip()) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onDetach (via callbacks)")
    }

    override fun onFragmentCreated(fm: FragmentManager, f: Fragment, savedInstanceState: Bundle?) {
        if (shouldSkip() ||
            // These methods are manually instrumented with duration.
            f is HomeFragment
        ) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onCreate (via callbacks)")
    }

    override fun onFragmentViewCreated(fm: FragmentManager, f: Fragment, v: View, savedInstanceState: Bundle?) {
        if (shouldSkip() ||
            // These methods are manually instrumented with duration.
            f is HomeFragment ||
            f is BaseBrowserFragment
        ) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onViewCreated (via callbacks)")
    }

    override fun onFragmentStarted(fm: FragmentManager, f: Fragment) {
        if (shouldSkip()) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onStart (via callbacks)")
    }

    override fun onFragmentResumed(fm: FragmentManager, f: Fragment) {
        if (shouldSkip()) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onResume (via callbacks)")
    }

    override fun onFragmentViewDestroyed(fm: FragmentManager, f: Fragment) {
        if (shouldSkip()) {
            return
        }

        engine.profiler?.addMarker(MARKER_NAME, "${f::class.simpleName}.onViewDestroyed (via callbacks)")
    }

    companion object {
        const val MARKER_NAME = "Fragment Lifecycle"

        fun register(supportFragmentManager: FragmentManager, engine: Engine) {
            val callbacks = MarkersFragmentLifecycleCallbacks(engine)

            // We do recursive because the NavHostFragment adds additional fragments as subfragments.
            supportFragmentManager.registerFragmentLifecycleCallbacks(callbacks, true)
        }
    }
}
