/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.mozonline

import android.app.Activity
import android.os.Bundle
import android.view.View
import android.webkit.WebView
import android.widget.ImageButton
import mozilla.components.concept.engine.EngineSession
import org.mozilla.fenix.R

/**
 * A special activity for displaying the detail content about privacy hyperlinked in alert dialog.
 */

class PrivacyContentDisplayActivity : Activity(), EngineSession.Observer {
    private lateinit var webView: WebView
    private lateinit var closeButton: ImageButton
    private var url: String? = ""
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_privacy_content_display)
        val addr = intent.extras
        if (addr != null) {
            url = addr.getString("url")
        }

        webView = findViewById<View>(R.id.privacyContentEngineView) as WebView
        webView.settings.javaScriptEnabled = true
        closeButton = findViewById<View>(R.id.privacyContentCloseButton) as ImageButton
    }

    override fun onStart() {
        super.onStart()

        url?.let {
            webView.loadUrl(it)
        }

        closeButton.setOnClickListener {
            finish()
        }
    }
}
