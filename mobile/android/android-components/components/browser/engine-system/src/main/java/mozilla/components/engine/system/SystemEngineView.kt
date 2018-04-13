/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.engine.system

import android.content.Context
import android.util.AttributeSet
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.FrameLayout
import mozilla.components.engine.EngineSession
import mozilla.components.engine.EngineView
import java.lang.ref.WeakReference

/**
 * WebView-based implementation of EngineView.
 */
class SystemEngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), EngineView {
    internal val currentWebView = createWebView(context)
    private var session: SystemEngineSession? = null

    init {
        // Currently this implementation supports only a single WebView. Eventually this
        // implementation should be able to maintain at least two WebView instances to be able to
        // animate the views when switching sessions.
        addView(currentWebView)
    }

    /**
     * Render the content of the given session.
     */
    override fun render(session: EngineSession) {
        val internalSession = session as SystemEngineSession
        this.session = internalSession

        internalSession.view = WeakReference(this)

        internalSession.urlToLoad?.let {
            currentWebView.loadUrl(internalSession.urlToLoad)
            internalSession.urlToLoad = null
        }
    }

    private fun createWebView(context: Context): WebView {
        val webView = WebView(context)

        webView.webViewClient = object : WebViewClient() {
            override fun onPageFinished(view: WebView?, url: String?) {
                url?.let {
                    session?.internalNotifyObservers { onLocationChange(it) }
                }
            }
        }

        return webView
    }
}
