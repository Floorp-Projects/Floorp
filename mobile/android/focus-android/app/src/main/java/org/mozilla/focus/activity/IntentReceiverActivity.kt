/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import mozilla.components.browser.session.Session
import mozilla.components.support.utils.SafeIntent
import org.mozilla.focus.ext.components
import org.mozilla.focus.session.IntentProcessor
import org.mozilla.focus.utils.SupportUtils

/**
 * This activity receives VIEW intents and either forwards them to MainActivity or CustomTabActivity.
 */
class IntentReceiverActivity : Activity() {
    private val intentProcessor by lazy { IntentProcessor(this, components.sessionManager) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = SafeIntent(intent)

        if (intent.dataString.equals(SupportUtils.OPEN_WITH_DEFAULT_BROWSER_URL)) {
            dispatchNormalIntent()
            return
        }

        val session = intentProcessor.handleIntent(this, intent, savedInstanceState)

        if (session?.isCustomTabSession() == true) {
            dispatchCustomTabsIntent(session)
        } else {
            dispatchNormalIntent()
        }

        finish()
    }

    private fun dispatchCustomTabsIntent(session: Session) {
        val intent = Intent(intent)

        intent.setClassName(applicationContext, CustomTabActivity::class.java.name)

        // We are adding a generated custom tab ID to the intent here. CustomTabActivity will
        // use this ID to later decide what session to display once it is created.
        intent.putExtra(CustomTabActivity.CUSTOM_TAB_ID, session.id)

        startActivity(intent)
    }

    private fun dispatchNormalIntent() {
        val intent = Intent(intent)
        intent.setClassName(applicationContext, MainActivity::class.java.name)
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP

        startActivity(intent)
    }
}
