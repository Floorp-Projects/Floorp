/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.ViewTreeObserver
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class TransitionPreDrawListener(
    private val fragment: Fragment,
    private val viewTreeObserver: ViewTreeObserver,
    private val restoreLayoutState: () -> Unit
) : ViewTreeObserver.OnPreDrawListener, LifecycleObserver {

    init {
        fragment.viewLifecycleOwner.lifecycle.addObserver(this)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_CREATE)
    fun onCreateView() {
        viewTreeObserver.addOnPreDrawListener(this)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun onDestroyView() {
        viewTreeObserver.removeOnPreDrawListener(this)
    }

    override fun onPreDraw(): Boolean {
        if (fragment.view != null) {
            fragment.viewLifecycleOwner.lifecycleScope.launch {
                delay(ANIM_SCROLL_DELAY)
                restoreLayoutState()
                fragment.startPostponedEnterTransition()
            }.invokeOnCompletion {
                viewTreeObserver.removeOnPreDrawListener(this)
            }
        }
        return true
    }

    companion object {
        private const val ANIM_SCROLL_DELAY = 100L
    }
}
