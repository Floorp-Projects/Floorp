package org.mozilla.fenix

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.ext.components

class IntentReceiverActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        components.utils.intentProcessor.process(intent)

        val intent = Intent(intent)
        if (CustomTabConfig.isCustomTabIntent(SafeIntent(intent))) {
            // TODO Enter CustomTabActivity here.
        } else {
            intent.setClassName(applicationContext, HomeActivity::class.java.name)
        }

        startActivity(intent)
        finish()
    }
}