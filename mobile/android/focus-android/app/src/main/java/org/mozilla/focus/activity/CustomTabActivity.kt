/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.content.Context
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.locale.LocaleAwareAppCompatActivity
import mozilla.components.support.utils.SafeIntent
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.updateSecureWindowFlags
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.telemetry.startuptelemetry.StartupPathProvider
import org.mozilla.focus.telemetry.startuptelemetry.StartupTypeTelemetry

/**
 * The main entry point for "custom tabs" opened by third-party apps.
 */
class CustomTabActivity : LocaleAwareAppCompatActivity() {
    private lateinit var customTabId: String
    private lateinit var browserFragment: BrowserFragment

    private val startupPathProvider = StartupPathProvider()
    private lateinit var startupTypeTelemetry: StartupTypeTelemetry

    override fun onCreate(savedInstanceState: Bundle?) {
        updateSecureWindowFlags()
        super.onCreate(savedInstanceState)

        val intent = SafeIntent(intent)
        val customTabId = intent.getStringExtra(CUSTOM_TAB_ID)

        // The session for this ID, no longer exists. This usually happens because we were gc-d
        // and since we do not save custom tab sessions, the activity is re-created and we no longer
        // have a session with us to restore. It's safer to finish the activity instead.
        if (customTabId == null || components.store.state.findCustomTab(customTabId) == null) {
            finish()
            return
        }

        this.customTabId = customTabId

        @Suppress("DEPRECATION") // https://github.com/mozilla-mobile/focus-android/issues/5016
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN

        setContentView(R.layout.activity_customtab)

        if (savedInstanceState == null || !this::browserFragment.isInitialized) {
            browserFragment = BrowserFragment.createForTab(customTabId)
            supportFragmentManager.beginTransaction()
                .add(R.id.container, browserFragment)
                .commit()
        }

        startupPathProvider.attachOnActivityOnCreate(lifecycle, intent.unsafe)
        startupTypeTelemetry = StartupTypeTelemetry(components.startupStateProvider, startupPathProvider).apply {
            attachOnMainActivityOnCreate(lifecycle)
        }
    }

    override fun onPause() {
        super.onPause()

        if (isFinishing) {
            components.customTabsUseCases.remove(customTabId)
        }
    }

    override fun onBackPressed() {
        if (browserFragment.sessionFeature.onBackPressed()) {
            return
        } else {
            super.onBackPressed()
        }
    }

    override fun onCreateView(parent: View?, name: String, context: Context, attrs: AttributeSet): View? {
        return if (name == EngineView::class.java.name) {
            val engineView = components.engine.createView(context, attrs)
            engineView.asView()
        } else super.onCreateView(parent, name, context, attrs)
    }

    companion object {
        const val CUSTOM_TAB_ID = "custom_tab_id"
    }
}
