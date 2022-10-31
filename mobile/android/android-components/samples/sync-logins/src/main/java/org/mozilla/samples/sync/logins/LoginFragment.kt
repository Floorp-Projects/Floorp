/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync.logins

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.webkit.CookieManager
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.fragment.app.Fragment

class LoginFragment : Fragment() {

    private lateinit var authUrl: String
    private lateinit var redirectUrl: String
    private var mWebView: WebView? = null
    private var listener: OnLoginCompleteListener? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        arguments?.let {
            authUrl = it.getString(AUTH_URL)!!
            redirectUrl = it.getString(REDIRECT_URL)!!
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view: View = inflater.inflate(R.layout.fragment_view, container, false)
        val webView = view.findViewById<WebView>(R.id.webview)
        // Need JS, cookies and localStorage.
        webView.settings.domStorageEnabled = true
        webView.settings.javaScriptEnabled = true
        CookieManager.getInstance().setAcceptCookie(true)

        webView.webViewClient = object : WebViewClient() {
            override fun onPageStarted(view: WebView?, url: String?, favicon: Bitmap?) {
                if (url != null && url.startsWith(redirectUrl)) {
                    val uri = Uri.parse(url)
                    val code = uri.getQueryParameter("code")
                    val state = uri.getQueryParameter("state")
                    val action = uri.getQueryParameter("action")
                    if (code != null && state != null && action != null) {
                        listener?.onLoginComplete(code, state, action, this@LoginFragment)
                    }
                }

                super.onPageStarted(view, url, favicon)
            }
        }
        webView.loadUrl(authUrl)

        mWebView?.destroy()
        mWebView = webView

        return view
    }

    @Suppress("TooGenericExceptionThrown")
    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is OnLoginCompleteListener) {
            listener = context
        } else {
            throw IllegalStateException("$context must implement OnLoginCompleteListener")
        }
    }

    override fun onDetach() {
        super.onDetach()
        listener = null
    }

    override fun onPause() {
        super.onPause()
        mWebView?.onPause()
    }

    override fun onResume() {
        super.onResume()
        mWebView?.onResume()
    }

    interface OnLoginCompleteListener {
        fun onLoginComplete(code: String, state: String, action: String, fragment: LoginFragment)
    }

    companion object {
        const val AUTH_URL = "authUrl"
        const val REDIRECT_URL = "redirectUrl"

        fun create(authUrl: String, redirectUrl: String): LoginFragment =
            LoginFragment().apply {
                arguments = Bundle().apply {
                    putString(AUTH_URL, authUrl)
                    putString(REDIRECT_URL, redirectUrl)
                }
            }
    }
}
