/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.os.Bundle
import org.mozilla.focus.customtabs.CustomTabConfig
import org.mozilla.focus.session.Session
import org.mozilla.focus.session.SessionManager
import org.mozilla.focus.utils.SafeIntent

/**
 * The main entry point for "custom tabs" opened by third-party apps.
 */
class CustomTabActivity : MainActivity() {
    private lateinit var customTabId: String

    override fun isCustomTabMode(): Boolean = true

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = SafeIntent(intent)
        customTabId = intent.getStringExtra(CustomTabConfig.EXTRA_CUSTOM_TAB_ID)
    }

    override fun onPause() {
        super.onPause()

        if (isFinishing) {
            // Remove custom tab session
            val sessionManager = SessionManager.getInstance()
            val session = sessionManager.getCustomTabSessionByCustomTabId(customTabId)

            if (session != null) {
                sessionManager.removeSession(session.uuid)
            }
        }
    }

    override fun getCurrentSessionForActivity(): Session =
            SessionManager.getInstance().getCustomTabSessionByCustomTabId(customTabId)
}
