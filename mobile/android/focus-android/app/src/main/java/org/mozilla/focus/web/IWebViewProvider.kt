/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.webkit.WebSettings
import org.mozilla.focus.webview.SystemWebView

interface IWebViewProvider {
    fun preload(context: Context)
    fun create(context: Context, attributeSet: AttributeSet?): View
    fun performCleanup(context: Context)
    fun performNewBrowserSessionCleanup()
    fun requestMobileSite(context: Context, webSettings: WebSettings)
    fun requestDesktopSite(webSettings: WebSettings)
    fun applyAppSettings(context: Context, webSettings: WebSettings, systemWebView: SystemWebView)
    fun disableBlocking(webSettings: WebSettings, systemWebView: SystemWebView)
    fun getUABrowserString(existingUAString: String, focusToken: String): String?
}
