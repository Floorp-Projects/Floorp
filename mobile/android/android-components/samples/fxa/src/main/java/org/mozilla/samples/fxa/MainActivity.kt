/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.fxa

import android.net.Uri
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.support.customtabs.CustomTabsIntent
import android.view.View
import android.content.Intent
import android.widget.TextView
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FxaResult
import mozilla.components.service.fxa.OAuthInfo
import mozilla.components.service.fxa.Profile


open class MainActivity : AppCompatActivity() {

    private var account: FirefoxAccount? = null
    private var scopes: Array<String> = arrayOf("profile")

    companion object {
        const val CLIENT_ID = "12cc4070a481bc73"
        const val REDIRECT_URL = "fxaclient://android.redirect"
        const val CONFIG_URL = "https://latest.dev.lcip.org"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        findViewById<View>(R.id.button).setOnClickListener {
            openOAuthTab()
        }

        Config.custom(CONFIG_URL).then { value: Config? ->
            value?.let {
                account = FirefoxAccount(it, CLIENT_ID)
                FxaResult.fromValue(account)
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        val action = intent.action
        val data = intent.dataString

        if (Intent.ACTION_VIEW == action && data != null) {
            val txtView: TextView = findViewById(R.id.txtView)
            val url = Uri.parse(data)
            val code = url.getQueryParameter("code")
            val state = url.getQueryParameter("state")

            val handleAuth = { _: OAuthInfo? -> account?.getProfile() }
            val handleProfile = { value: Profile? ->
                value?.let {
                    runOnUiThread {
                        txtView.text = getString(R.string.signed_in, "${it.displayName ?: ""} ${it.email}")
                    }
                }
                FxaResult<Void>()
            }
            account?.completeOAuthFlow(code, state)?.then(handleAuth)?.then(handleProfile)
        }
    }

    private fun openTab(url: String) {
        val customTabsIntent = CustomTabsIntent.Builder()
                .addDefaultShareMenuItem()
                .setShowTitle(true)
                .build()

        customTabsIntent.intent.data = Uri.parse(url)
        customTabsIntent.launchUrl(this@MainActivity, Uri.parse(url))
    }

    private fun openOAuthTab() {
        val valueListener = { value: String? ->
            value?.let { openTab(it) }
            FxaResult<Void>()
        }

        account?.beginOAuthFlow(REDIRECT_URL, scopes, false)?.then(valueListener)
    }
}
