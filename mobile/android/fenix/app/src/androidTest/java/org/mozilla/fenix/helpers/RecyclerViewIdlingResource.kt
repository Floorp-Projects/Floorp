/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.util.Log
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.IdlingResource.ResourceCallback
import org.mozilla.fenix.helpers.Constants.TAG

class RecyclerViewIdlingResource(private val recycler: androidx.recyclerview.widget.RecyclerView, val minItemCount: Int = 0) :
    IdlingResource {

    private var callback: ResourceCallback? = null

    override fun isIdleNow(): Boolean {
        if (recycler.adapter != null && recycler.adapter!!.itemCount >= minItemCount) {
            if (callback != null) {
                Log.i(TAG, "RecyclerViewIdlingResource: Trying to verify that the resource transitioned from busy to idle")
                callback!!.onTransitionToIdle()
                Log.i(TAG, "RecyclerViewIdlingResource: The resource transitioned to idle")
            }
            return true
        }
        return false
    }

    override fun registerIdleTransitionCallback(callback: ResourceCallback) {
        this.callback = callback
        Log.i(TAG, "RecyclerViewIdlingResource: Notified asynchronously that the resource is transitioning from busy to idle")
    }

    override fun getName(): String {
        Log.i(TAG, "RecyclerViewIdlingResource: Trying to return the the name of the resource: ${RecyclerViewIdlingResource::class.java.name + ":" + recycler.id}")
        return RecyclerViewIdlingResource::class.java.name + ":" + recycler.id
    }
}
