/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.ComponentCallbacks2
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.feature.intent.ext.getSessionId
import mozilla.components.browser.tabstray.BrowserTabsTray
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.support.utils.SafeIntent
import org.mozilla.samples.browser.addons.WebExtensionActionPopupActivity
import org.mozilla.samples.browser.ext.components

/**
 * Activity that holds the [BrowserFragment].
 */
open class BrowserActivity : AppCompatActivity(), ComponentCallbacks2 {
    private var webExtScope: CoroutineScope? = null

    /**
     * Returns a new instance of [BrowserFragment] to display.
     */
    open fun createBrowserFragment(sessionId: String?): Fragment =
        BrowserFragment.create(sessionId)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        if (savedInstanceState == null) {
            val sessionId = SafeIntent(intent).getSessionId()
            supportFragmentManager.beginTransaction().apply {
                replace(R.id.container, createBrowserFragment(sessionId))
                commit()
            }
        }
    }

    @kotlinx.coroutines.ExperimentalCoroutinesApi
    override fun onStart() {
        super.onStart()
        webExtScope = observeWebExtensionPopups()
    }

    override fun onStop() {
        super.onStop()
        webExtScope?.cancel()
    }

    override fun onDestroy() {
        super.onDestroy()

        components.historyStorage.cleanup()
    }

    override fun onBackPressed() {
        supportFragmentManager.fragments.forEach {
            if (it is UserInteractionHandler && it.onBackPressed()) {
                return
            }
        }

        super.onBackPressed()
    }

    override fun onCreateView(parent: View?, name: String, context: Context, attrs: AttributeSet): View? =
        when (name) {
            EngineView::class.java.name -> components.engine.createView(context, attrs).asView()
            TabsTray::class.java.name -> BrowserTabsTray(context, attrs)
            else -> super.onCreateView(parent, name, context, attrs)
        }

    override fun onTrimMemory(level: Int) {
        components.sessionManager.onLowMemory()
        components.icons.onLowMemory()
    }

    @kotlinx.coroutines.ExperimentalCoroutinesApi
    private fun observeWebExtensionPopups(): CoroutineScope {
        return components.store.flowScoped { flow ->
            flow.ifChanged { it.extensions }
                .map { it.extensions.filterValues { extension -> extension.popupSession != null } }
                .ifChanged()
                .collect { extensionStates ->
                    if (extensionStates.values.isNotEmpty()) {
                        // We currently limit to one active popup session at a time
                        val webExtensionState = extensionStates.values.first()

                        val intent = Intent(this, WebExtensionActionPopupActivity::class.java)
                        intent.putExtra("web_extension_id", webExtensionState.id)
                        intent.putExtra("web_extension_name", webExtensionState.name)
                        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                        startActivity(intent)
                    }
                }
        }
    }
}
