/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.os.Bundle
import mozilla.components.utils.SafeIntent
import org.mozilla.focus.customtabs.CustomTabConfig
import org.mozilla.focus.session.Session
import org.mozilla.focus.session.SessionManager

/**
 * The main entry point for "custom tabs" opened by third-party apps.
 */
class CustomTabActivity : MainActivity() {
    private lateinit var customTabId: String
    private val customTabSession: Session by lazy {
        SessionManager.getInstance().getCustomTabSessionByCustomTabIdOrThrow(customTabId)
    }

    override fun isCustomTabMode(): Boolean = true

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = SafeIntent(intent)
        customTabId = intent.getStringExtra(CustomTabConfig.EXTRA_CUSTOM_TAB_ID)
                ?: throw IllegalAccessError("No custom tab id in intent")
    }

    override fun onPause() {
        super.onPause()

        if (isFinishing) {
            // Remove custom tab session

            val sessionManager: SessionManager = SessionManager.getInstance()

            sessionManager
                    .getCustomTabSessionByCustomTabId(customTabId)
                    ?.let { sessionManager.removeCustomTabSession(it.uuid) }
        }
    }

    override fun getCurrentSessionForActivity() = customTabSession
}
