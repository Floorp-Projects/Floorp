/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.mozonline

import android.app.Activity
import android.content.Context
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import android.widget.ImageButton
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.contextmenu.DefaultSelectionActionDelegate
import mozilla.components.feature.search.BrowserStoreSearchAdapter
import mozilla.components.support.ktx.android.content.call
import mozilla.components.support.ktx.android.content.email
import mozilla.components.support.ktx.android.content.share
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

/**
 * A special activity for displaying the detail content about privacy hyperlinked in alert dialog.
 */

class PrivacyContentDisplayActivity : Activity(), EngineSession.Observer {
    private lateinit var engineView: EngineView
    private lateinit var closeButton: ImageButton
    private lateinit var engineSession: EngineSession
    private var url: String? = ""
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_privacy_content_display)
        val addr = intent.extras
        if (addr != null) {
            url = addr.getString("url")
        }

        engineView = findViewById<View>(R.id.privacyContentEngineView) as EngineView
        closeButton = findViewById<View>(R.id.privacyContentCloseButton) as ImageButton
        engineSession = components.core.engine.createSession()
    }

    override fun onCreateView(
        parent: View?,
        name: String,
        context: Context,
        attrs: AttributeSet
    ): View? = when (name) {
        EngineView::class.java.name -> components.core.engine.createView(context, attrs).apply {
            selectionActionDelegate = DefaultSelectionActionDelegate(
                BrowserStoreSearchAdapter(
                    components.core.store
                ),
                resources = context.resources,
                shareTextClicked = { share(it) },
                emailTextClicked = { email(it) },
                callTextClicked = { call(it) }
            )
        }.asView()
        else -> super.onCreateView(parent, name, context, attrs)
    }

    override fun onStart() {
        super.onStart()
        engineSession.register(this)
        engineSession.let { engineSession ->
            engineView.render(engineSession)
            url?.let { engineSession.loadUrl(it) }
        }
        closeButton.setOnClickListener { finish() }
    }

    override fun onStop() {
        super.onStop()
        engineSession.unregister(this)
    }

    override fun onDestroy() {
        super.onDestroy()
        engineSession.close()
    }
}
