/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.ComponentCallbacks2
import android.content.Context
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.util.AttributeSet
import android.view.View
import mozilla.components.browser.tabstray.BrowserTabsTray
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.session.SessionIntentProcessor
import mozilla.components.support.utils.SafeIntent
import org.mozilla.samples.browser.ext.components

open class BrowserActivity : AppCompatActivity(), ComponentCallbacks2 {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        if (savedInstanceState == null) {
            val sessionId = SafeIntent(intent).getStringExtra(SessionIntentProcessor.ACTIVE_SESSION_ID)
            supportFragmentManager?.beginTransaction()?.apply {
                replace(R.id.container, BrowserFragment.create(sessionId))
                commit()
            }
        }
    }

    override fun onBackPressed() {
        supportFragmentManager.fragments.forEach {
            if (it is BackHandler && it.onBackPressed()) {
                return
            }
        }

        super.onBackPressed()
    }

    override fun onCreateView(parent: View?, name: String?, context: Context, attrs: AttributeSet?): View? =
        when (name) {
            EngineView::class.java.name -> components.engine.createView(context, attrs).asView()
            TabsTray::class.java.name -> BrowserTabsTray(context, attrs)
            else -> super.onCreateView(parent, name, context, attrs)
        }

    override fun onTrimMemory(level: Int) {
        components.sessionManager.onLowMemory()
    }
}
