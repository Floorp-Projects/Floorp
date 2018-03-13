/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import mozilla.components.utils.SafeIntent
import org.mozilla.focus.customtabs.CustomTabConfig
import java.util.UUID

/**
 * This activity receives VIEW intents and either forwards them to MainActivity or CustomTabActivity.
 */
class IntentReceiverActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = SafeIntent(intent)

        if (CustomTabConfig.isCustomTabIntent(intent)) {
            dispatchCustomTabsIntent()
        } else {
            dispatchNormalIntent()
        }

        finish()
    }

    private fun dispatchCustomTabsIntent() {
        val intent = Intent(intent)

        intent.setClassName(applicationContext, CustomTabActivity::class.java.name)

        // We are adding a generated custom tab ID to the intent here. CustomTabActivity will
        // use this ID to later decide what session to display once it is created.
        intent.putExtra(CustomTabConfig.EXTRA_CUSTOM_TAB_ID, UUID.randomUUID().toString())

        startActivity(intent)
    }

    private fun dispatchNormalIntent() {
        val intent = Intent(intent)
        intent.setClassName(applicationContext, MainActivity::class.java.name)

        startActivity(intent)
    }
}
