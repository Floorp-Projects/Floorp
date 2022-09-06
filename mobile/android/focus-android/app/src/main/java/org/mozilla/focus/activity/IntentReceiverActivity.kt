/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import mozilla.components.feature.intent.ext.sanitize
import mozilla.components.feature.search.widget.BaseVoiceSearchActivity
import mozilla.components.support.utils.toSafeIntent
import org.mozilla.focus.ext.components
import org.mozilla.focus.session.IntentProcessor
import org.mozilla.focus.utils.SupportUtils

/**
 * This activity receives VIEW intents and either forwards them to MainActivity or CustomTabActivity.
 */
class IntentReceiverActivity : Activity() {
    private val intentProcessor by lazy {
        IntentProcessor(this, components.tabsUseCases, components.customTabsUseCases)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = intent.sanitize().toSafeIntent()

        if (intent.dataString.equals(SupportUtils.OPEN_WITH_DEFAULT_BROWSER_URL)) {
            dispatchNormalIntent()
            return
        }

        val result = intentProcessor.handleIntent(this, intent, savedInstanceState)
        if (result is IntentProcessor.Result.CustomTab) {
            dispatchCustomTabsIntent(result.id)
        } else {
            dispatchNormalIntent()
        }

        finish()
    }

    private fun dispatchCustomTabsIntent(tabId: String) {
        val intent = Intent(intent)

        intent.setClassName(applicationContext, CustomTabActivity::class.java.name)

        // We are adding a generated custom tab ID to the intent here. CustomTabActivity will
        // use this ID to later decide what session to display once it is created.
        intent.putExtra(CustomTabActivity.CUSTOM_TAB_ID, tabId)

        startActivity(intent)
    }

    private fun dispatchNormalIntent() {
        val intent = Intent(intent)
        intent.setClassName(applicationContext, MainActivity::class.java.name)
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
        intent.putExtra(SEARCH_WIDGET_EXTRA, intent.getBooleanExtra(SEARCH_WIDGET_EXTRA, false))
        intent.putExtra(
            BaseVoiceSearchActivity.SPEECH_PROCESSING,
            intent.getStringExtra(BaseVoiceSearchActivity.SPEECH_PROCESSING),
        )
        startActivity(intent)
    }

    companion object {
        const val SEARCH_WIDGET_EXTRA = "search_widget_extra"
    }
}
