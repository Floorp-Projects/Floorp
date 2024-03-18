/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.helpers

import androidx.test.core.app.ApplicationProvider
import androidx.test.espresso.IdlingResource
import mozilla.components.browser.state.selector.selectedTab
import org.mozilla.fenix.FenixApplication

/**
 * An IdlingResource implementation that waits until the current session is not loading anymore.
 * Only after loading has completed further actions will be performed.
 */

class SessionLoadedIdlingResource : IdlingResource {
    private var resourceCallback: IdlingResource.ResourceCallback? = null

    override fun getName(): String {
        return SessionLoadedIdlingResource::class.java.simpleName
    }

    override fun isIdleNow(): Boolean {
        val context = ApplicationProvider.getApplicationContext<FenixApplication>()
        val selectedTab = context.components.core.store.state.selectedTab

        return if (selectedTab?.content?.loading == true) {
            false
        } else {
            if (selectedTab?.content?.progress == 100) {
                invokeCallback()
                true
            } else {
                false
            }
        }
    }

    private fun invokeCallback() {
        if (resourceCallback != null) {
            resourceCallback!!.onTransitionToIdle()
        }
    }

    override fun registerIdleTransitionCallback(callback: IdlingResource.ResourceCallback) {
        this.resourceCallback = callback
    }
}
