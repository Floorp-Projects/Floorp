/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.os.Bundle
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.support.utils.SafeIntent
import org.mozilla.focus.ext.components

/**
 * The main entry point for "custom tabs" opened by third-party apps.
 */
class CustomTabActivity : MainActivity() {
    private lateinit var customTabId: String

    override val currentTabForActivity: CustomTabSessionState by lazy {
        components.store.state.findCustomTab(customTabId)
            ?: throw IllegalAccessError("No custom tab with id $customTabId")
    }

    override val isCustomTabMode: Boolean = true

    override fun onCreate(savedInstanceState: Bundle?) {
        val intent = SafeIntent(intent)
        customTabId = intent.getStringExtra(CUSTOM_TAB_ID)
            ?: throw IllegalAccessError("No custom tab id in intent")

        super.onCreate(savedInstanceState)
    }

    override fun onPause() {
        super.onPause()

        if (isFinishing) {
            components.customTabsUseCases.remove(currentTabForActivity.id)
        }
    }
    companion object {
        const val CUSTOM_TAB_ID = "custom_tab_id"
    }
}
