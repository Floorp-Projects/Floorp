/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.util.Log
import android.view.View
import androidx.test.espresso.IdlingResource
import org.mozilla.fenix.helpers.Constants.TAG

class ViewVisibilityIdlingResource(
    private val view: View,
    private val expectedVisibility: Int,
) : IdlingResource {
    private var resourceCallback: IdlingResource.ResourceCallback? = null
    private var isIdle: Boolean = false

    override fun getName(): String {
        Log.i(TAG, "ViewVisibilityIdlingResource: Trying to return the the name of the resource: ${ViewVisibilityIdlingResource::class.java.name + ":" + view.id + ":" + expectedVisibility}")
        return ViewVisibilityIdlingResource::class.java.name + ":" + view.id + ":" + expectedVisibility
    }

    override fun isIdleNow(): Boolean {
        if (isIdle) return true

        isIdle = view.visibility == expectedVisibility

        if (isIdle) {
            Log.i(TAG, "ViewVisibilityIdlingResource: Trying to verify that the resource transitioned from busy to idle")
            resourceCallback?.onTransitionToIdle()
            Log.i(TAG, "ViewVisibilityIdlingResource: The resource transitioned to idle")
        }

        return isIdle
    }

    override fun registerIdleTransitionCallback(callback: IdlingResource.ResourceCallback?) {
        this.resourceCallback = callback
        Log.i(TAG, "ViewVisibilityIdlingResource: Notified asynchronously that the resource is transitioning from busy to idle")
    }
}
