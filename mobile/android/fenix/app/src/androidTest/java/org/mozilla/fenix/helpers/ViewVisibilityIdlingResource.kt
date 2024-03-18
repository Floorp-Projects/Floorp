/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.view.View
import androidx.test.espresso.IdlingResource

class ViewVisibilityIdlingResource(
    private val view: View,
    private val expectedVisibility: Int,
) : IdlingResource {
    private var resourceCallback: IdlingResource.ResourceCallback? = null
    private var isIdle: Boolean = false

    override fun getName(): String {
        return ViewVisibilityIdlingResource::class.java.name + ":" + view.id + ":" + expectedVisibility
    }

    override fun isIdleNow(): Boolean {
        if (isIdle) return true

        isIdle = view.visibility == expectedVisibility

        if (isIdle) {
            resourceCallback?.onTransitionToIdle()
        }

        return isIdle
    }

    override fun registerIdleTransitionCallback(callback: IdlingResource.ResourceCallback?) {
        this.resourceCallback = callback
    }
}
