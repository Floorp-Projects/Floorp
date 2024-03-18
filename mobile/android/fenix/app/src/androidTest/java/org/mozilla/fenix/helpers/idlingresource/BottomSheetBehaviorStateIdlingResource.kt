/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers.idlingresource

import android.view.View
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.IdlingResource.ResourceCallback
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetBehavior.BottomSheetCallback

class BottomSheetBehaviorStateIdlingResource(behavior: BottomSheetBehavior<*>) :
    BottomSheetCallback(), IdlingResource {

    private var isIdle: Boolean
    private var callback: ResourceCallback? = null

    override fun onStateChanged(bottomSheet: View, newState: Int) {
        val wasIdle = isIdle
        isIdle = isIdleState(newState)
        if (!wasIdle && isIdle && callback != null) {
            callback!!.onTransitionToIdle()
        }
    }

    override fun onSlide(bottomSheet: View, slideOffset: Float) {
        // no-op
    }

    override fun getName(): String {
        return BottomSheetBehaviorStateIdlingResource::class.java.simpleName
    }

    override fun isIdleNow(): Boolean {
        return isIdle
    }

    override fun registerIdleTransitionCallback(callback: ResourceCallback) {
        this.callback = callback
    }

    private fun isIdleState(state: Int): Boolean {
        return state != BottomSheetBehavior.STATE_DRAGGING &&
            state != BottomSheetBehavior.STATE_SETTLING &&
            // When detecting STATE_HALF_EXPANDED we immediately transit to STATE_HIDDEN.
            // Consider this also an intermediary state so not idling.
            state != BottomSheetBehavior.STATE_HALF_EXPANDED
    }

    init {
        behavior.addBottomSheetCallback(this)
        val state = behavior.state
        isIdle = isIdleState(state)
    }
}
