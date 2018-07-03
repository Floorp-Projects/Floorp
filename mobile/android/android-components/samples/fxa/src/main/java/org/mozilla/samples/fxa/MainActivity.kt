/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.fxa

import android.net.Uri
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import mozilla.components.service.fxa.FirefoxAccount
import android.support.customtabs.CustomTabsIntent
import android.view.View
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FxaClient
import android.content.Intent
import android.widget.TextView

open class MainActivity : AppCompatActivity() {

    private var account: FirefoxAccount? = null
    private var config: Config? = null

    companion object {
        const val CLIENT_ID = "12cc4070a481bc73"
        const val REDIRECT_URL = "fxaclient://android.redirect"
        const val CONFIG_URL = "https://latest.dev.lcip.org"

        init {
            FxaClient.init()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        config = Config.custom(CONFIG_URL)
        config?.let {
            account = FirefoxAccount(it, CLIENT_ID)

            val btn = findViewById<View>(R.id.button)
            btn.setOnClickListener {
                account?.beginOAuthFlow(REDIRECT_URL, arrayOf("profile"), false)?.let {
                    openAuthTab(it)
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        val action = intent.action
        val data = intent.dataString

        if (Intent.ACTION_VIEW == action && data != null) {
            val txtView: TextView = findViewById(R.id.txtView)
            val info = authenticate(data)
            txtView.text = getString(R.string.signed_in, info)
        }
    }

    private fun authenticate(redirectUrl: String): String? {
        val url = Uri.parse(redirectUrl)
        val code = url.getQueryParameter("code")
        val state = url.getQueryParameter("state")

        account?.completeOAuthFlow(code, state)
        val profile = account?.getProfile()
        return "${profile?.displayName ?: ""} ${profile?.email}"
    }

    private fun openAuthTab(url: String) {
        val customTabsIntent = CustomTabsIntent.Builder()
                .addDefaultShareMenuItem()
                .setShowTitle(true)
                .build()

        customTabsIntent.intent.data = Uri.parse(url)
        customTabsIntent.launchUrl(this@MainActivity, Uri.parse(url))
    }
}
