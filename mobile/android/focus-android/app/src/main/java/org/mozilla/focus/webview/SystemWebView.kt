/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.webview

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.preference.PreferenceManager
import android.support.annotation.VisibleForTesting
import android.util.AttributeSet
import android.util.Log
import android.util.SparseArray
import android.view.View
import android.view.autofill.AutofillValue
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.webkit.CookieManager
import android.webkit.DownloadListener
import android.webkit.WebChromeClient
import android.webkit.WebStorage
import android.webkit.WebView
import android.webkit.WebViewDatabase
import mozilla.components.support.utils.ThreadUtils
import org.mozilla.focus.BuildConfig
import org.mozilla.focus.session.Session
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.FileUtils
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.focus.utils.ViewUtils
import org.mozilla.focus.web.Download
import org.mozilla.focus.web.IFindListener
import org.mozilla.focus.web.IWebView
import org.mozilla.focus.web.WebViewProvider
import java.util.HashMap

@Suppress("TooManyFunctions")
class SystemWebView(context: Context, attrs: AttributeSet) : NestedWebView(context, attrs), IWebView,
    SharedPreferences.OnSharedPreferenceChangeListener {

    private var callback: IWebView.Callback? = null
    private val client: FocusWebViewClient = FocusWebViewClient(getContext().applicationContext)
    private val linkHandler: LinkHandler

    init {

        webViewClient = client
        webChromeClient = createWebChromeClient()
        setDownloadListener(createDownloadListener())

        if (BuildConfig.DEBUG) {
            WebView.setWebContentsDebuggingEnabled(true)
        }

        isLongClickable = true

        linkHandler = LinkHandler(this)
        setOnLongClickListener(linkHandler)
    }

    @VisibleForTesting
    fun getCallback(): IWebView.Callback? {
        return callback
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        PreferenceManager.getDefaultSharedPreferences(context).registerOnSharedPreferenceChangeListener(this)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            TelemetryAutofillCallback.register(context)
        }
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        PreferenceManager.getDefaultSharedPreferences(context).unregisterOnSharedPreferenceChangeListener(this)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            TelemetryAutofillCallback.unregister(context)
        }
    }

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection? {
        val connection: InputConnection? = super.onCreateInputConnection(outAttrs)
        outAttrs.imeOptions = outAttrs.imeOptions or ViewUtils.IME_FLAG_NO_PERSONALIZED_LEARNING
        return connection
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        WebViewProvider.applyAppSettings(context, settings, this)
    }

    override fun onPause() {
        super.onPause()
        pauseTimers()
    }

    override fun onResume() {
        super.onResume()
        resumeTimers()
    }

    override fun restoreWebViewState(session: Session) {
        val stateData = session.webViewState

        val backForwardList = if (stateData != null)
            super.restoreState(stateData)
        else
            null

        val desiredURL = session.url.value

        client.restoreState(stateData)
        client.notifyCurrentURL(desiredURL)

        // Pages are only added to the back/forward list when loading finishes. If a new page is
        // loading when the Activity is paused/killed, then that page won't be in the list,
        // and needs to be restored separately to the history list. We detect this by checking
        // whether the last fully loaded page (getCurrentItem()) matches the last page that the
        // WebView was actively loading (which was retrieved during onSaveInstanceState():
        // WebView.getUrl() always returns the currently loading or loaded page).
        // If the app is paused/killed before the initial page finished loading, then the entire
        // list will be null - so we need to additionally check whether the list even exists.

        if (backForwardList != null && backForwardList.currentItem!!.url == desiredURL) {
            // restoreState doesn't actually load the current page, it just restores navigation history,
            // so we also need to explicitly reload in this case:
            reload()
        } else {
            loadUrl(desiredURL)
        }
    }

    override fun saveWebViewState(session: Session) {
        // We store the actual state into another bundle that we will keep in memory as long as this
        // browsing session is active. The data that WebView stores in this bundle is too large for
        // Android to save and restore as part of the state bundle.
        val stateData = Bundle()

        super.saveState(stateData)
        client.saveState(this, stateData)

        session.saveWebViewState(stateData)
    }

    override fun setBlockingEnabled(enabled: Boolean) {
        client.isBlockingEnabled = enabled
        if (enabled) {
            WebViewProvider.applyAppSettings(context, settings, this)
        } else {
            WebViewProvider.disableBlocking(settings, this)
        }

        if (callback != null) {
            callback!!.onBlockingStateChanged(enabled)
        }
    }

    override fun setRequestDesktop(shouldRequestDesktop: Boolean) {
        if (shouldRequestDesktop) {
            WebViewProvider.requestDesktopSite(settings)
        } else {
            WebViewProvider.requestMobileSite(context, settings)
        }

        if (callback != null) {
            callback!!.onRequestDesktopStateChanged(shouldRequestDesktop)
        }
    }

    override fun setCallback(callback: IWebView.Callback?) {
        this.callback = callback
        client.setCallback(callback)
        linkHandler.setCallback(callback)
    }

    override fun setFindListener(findListener: IFindListener) {
        this.setFindListener(findListener as WebView.FindListener)
    }

    override fun loadUrl(url: String?) {
        // We need to check external URL handling here - shouldOverrideUrlLoading() is only
        // called by webview when clicking on a link, and not when opening a new page for the
        // first time using loadUrl().
        @Suppress("DEPRECATION")
        if (!client.shouldOverrideUrlLoading(this, url)) {
            val additionalHeaders = HashMap<String, String>()
            additionalHeaders["X-Requested-With"] = ""

            super.loadUrl(url, additionalHeaders)
        }

        client.notifyCurrentURL(url)
    }

    override fun exitFullscreen() {}

    override fun loadData(baseURL: String, data: String, mimeType: String, encoding: String, historyURL: String) {
        loadDataWithBaseURL(baseURL, data, mimeType, encoding, historyURL)
    }

    override fun destroy() {
        super.destroy()

        // WebView might save data to disk once it gets destroyed. In this case our cleanup call
        // might not have been able to see this data. Let's do it again.
        deleteContentFromKnownLocations(context)
    }

    override fun cleanup() {
        clearFormData()
        clearHistory()
        clearMatches()
        clearSslPreferences()
        clearCache(true)

        // We don't care about the callback - we just want to make sure cookies are gone
        CookieManager.getInstance().removeAllCookies(null)

        WebStorage.getInstance().deleteAllData()

        val webViewDatabase = WebViewDatabase.getInstance(context)
        // It isn't entirely clear how this differs from WebView.clearFormData()
        @Suppress("DEPRECATION")
        webViewDatabase.clearFormData()
        webViewDatabase.clearHttpAuthUsernamePassword()

        deleteContentFromKnownLocations(context)
    }

    override fun autofill(values: SparseArray<AutofillValue>) {
        super.autofill(values)

        TelemetryWrapper.autofillPerformedEvent()
    }

    private fun createWebChromeClient(): WebChromeClient {
        return object : WebChromeClient() {
            override fun onProgressChanged(view: WebView, newProgress: Int) {
                if (callback != null) {
                    // This is the earliest point where we might be able to confirm a redirected
                    // URL: we don't necessarily get a shouldInterceptRequest() after a redirect,
                    // so we can only check the updated url in onProgressChanges(), or in onPageFinished()
                    // (which is even later).
                    val viewURL = view.url
                    if (!UrlUtils.isInternalErrorURL(viewURL) && viewURL != null) {
                        callback!!.onURLChanged(viewURL)
                        callback!!.onTitleChanged(title)
                    }
                    callback!!.onProgress(newProgress)
                }
            }

            override fun onShowCustomView(view: View, webviewCallback: WebChromeClient.CustomViewCallback) {
                val fullscreenCallback = IWebView.FullscreenCallback { webviewCallback.onCustomViewHidden() }

                callback!!.onEnterFullScreen(fullscreenCallback, view)
            }

            override fun onHideCustomView() {
                callback!!.onExitFullScreen()
            }
        }
    }

    private fun createDownloadListener(): DownloadListener {
        return DownloadListener { url, userAgent, contentDisposition, mimetype, contentLength ->
            if (!AppConstants.supportsDownloadingFiles()) {
                return@DownloadListener
            }

            val scheme = Uri.parse(url).scheme
            if (scheme == null || scheme != "http" && scheme != "https") {
                // We are ignoring everything that is not http or https. This is a limitation of
                // Android's download manager. There's no reason to show a download dialog for
                // something we can't download anyways.
                Log.w(TAG, "Ignoring download from non http(s) URL: $url")
                return@DownloadListener
            }

            if (callback != null) {
                val download = Download(
                    url,
                    userAgent,
                    contentDisposition,
                    mimetype,
                    contentLength,
                    Environment.DIRECTORY_DOWNLOADS,
                    null
                )
                callback!!.onDownloadStart(download)
            }
        }
    }

    companion object {
        private const val TAG = "WebkitView"

        fun deleteContentFromKnownLocations(context: Context) {
            ThreadUtils.postToBackgroundThread(Runnable {
                // We call all methods on WebView to delete data. But some traces still remain
                // on disk. This will wipe the whole webview directory.
                FileUtils.deleteWebViewDirectory(context)

                // WebView stores some files in the cache directory. We do not use it ourselves
                // so let's truncate it.
                FileUtils.truncateCacheDirectory(context)
            })
        }
    }
}
