/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers.idlingresource

import android.view.View
import android.view.View.VISIBLE
import androidx.fragment.app.FragmentManager
import androidx.test.espresso.IdlingResource
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.AddonsManagementFragment

class AddonsLoadingIdlingResource(val fragmentManager: FragmentManager) : IdlingResource {
    private var resourceCallback: IdlingResource.ResourceCallback? = null

    override fun getName(): String {
        return this::javaClass.name
    }

    override fun isIdleNow(): Boolean {
        val idle = addonsFinishedLoading()
        if (idle) {
            resourceCallback?.onTransitionToIdle()
        }
        return idle
    }

    override fun registerIdleTransitionCallback(callback: IdlingResource.ResourceCallback?) {
        if (callback != null) {
            resourceCallback = callback
        }
    }

    private fun addonsFinishedLoading(): Boolean {
        val progressbar = fragmentManager.findFragmentById(R.id.container)?.let {
            val addonsManagementFragment =
                it.childFragmentManager.fragments.first { it is AddonsManagementFragment }
            addonsManagementFragment.view?.findViewById<View>(R.id.add_ons_progress_bar)
        } ?: return true

        if (progressbar.visibility == VISIBLE) {
            return false
        }

        return true
    }
}
