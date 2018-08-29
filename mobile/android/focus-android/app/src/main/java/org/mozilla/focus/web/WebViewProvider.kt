/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.webkit.WebSettings
import org.mozilla.focus.utils.geckoEngineExperimentDescriptor
import org.mozilla.focus.utils.isInExperiment
import org.mozilla.focus.webview.SystemWebView

const val ENGINE_PREF_STRING_KEY = "use_gecko_engine"

object WebViewProvider : IWebViewProvider {

    var engine: IWebViewProvider? = null

    fun determineEngine(context: Context): IWebViewProvider {
        val useNewRenderer =
            if (context.isInExperiment(geckoEngineExperimentDescriptor))
                    true
                else
                    Config.DEFAULT_NEW_RENDERER
        engine = when (useNewRenderer) {
            false -> ClassicWebViewProvider()
            true -> GeckoWebViewProvider()
        }
        return engine!!
    }

    override fun preload(context: Context) {
        engine!!.preload(context)
    }

    override fun create(context: Context, attributeSet: AttributeSet?): View {
        return engine!!.create(context, attributeSet)
    }

    override fun performCleanup(context: Context) {
        engine!!.performCleanup(context)
    }

    override fun performNewBrowserSessionCleanup() {
        engine!!.performNewBrowserSessionCleanup()
    }

    override fun requestMobileSite(context: Context, webSettings: WebSettings) {
        engine!!.requestMobileSite(context, webSettings)
    }

    override fun requestDesktopSite(webSettings: WebSettings) {
        engine!!.requestDesktopSite(webSettings)
    }

    override fun getUABrowserString(existingUAString: String, focusToken: String): String {
        return engine!!.getUABrowserString(existingUAString, focusToken)
    }

    override fun applyAppSettings(context: Context, webSettings: WebSettings, systemWebView: SystemWebView) {
        engine!!.applyAppSettings(context, webSettings, systemWebView)
    }

    override fun disableBlocking(webSettings: WebSettings, systemWebView: SystemWebView) {
        engine!!.disableBlocking(webSettings, systemWebView)
    }
}
