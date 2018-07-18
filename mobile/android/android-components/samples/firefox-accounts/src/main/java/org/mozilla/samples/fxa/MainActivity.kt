/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.fxa

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.support.customtabs.CustomTabsIntent
import android.view.View
import android.content.Intent
import android.widget.CheckBox
import android.widget.TextView
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FxaResult
import mozilla.components.service.fxa.OAuthInfo
import mozilla.components.service.fxa.Profile

open class MainActivity : AppCompatActivity(), LoginFragment.OnLoginCompleteListener {

    private var account: FirefoxAccount? = null
    private var scopes: Array<String> = arrayOf("profile")
    private var wantsKeys: Boolean = false

    companion object {
        const val CLIENT_ID = "12cc4070a481bc73"
        const val REDIRECT_URL = "fxaclient://android.redirect"
        const val CONFIG_URL = "https://latest.dev.lcip.org"
        const val FXA_STATE_PREFS_KEY = "fxaAppState"
        const val FXA_STATE_KEY = "fxaState"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE).getString(FXA_STATE_KEY, "").let {
            FirefoxAccount.fromJSONString(it).then({
                this.account = it
                account?.getProfile()
            }, {
                Config.custom(CONFIG_URL).whenComplete { value: Config ->
                    account = FirefoxAccount(value, CLIENT_ID, REDIRECT_URL)
                }
                FxaResult()
            }).whenComplete {
                val txtView: TextView = findViewById(R.id.txtView)
                runOnUiThread {
                    txtView.text = getString(R.string.signed_in, "${it.displayName ?: ""} ${it.email}")
                }
            }
        }

        findViewById<View>(R.id.buttonCustomTabs).setOnClickListener {
            account?.beginOAuthFlow(scopes, wantsKeys)?.whenComplete { openTab(it) }
        }

        findViewById<View>(R.id.buttonWebView).setOnClickListener {
            account?.beginOAuthFlow(scopes, wantsKeys)?.whenComplete { openWebView(it) }
        }

        findViewById<View>(R.id.buttonLogout).setOnClickListener {
            getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE).edit().putString(FXA_STATE_KEY, "").apply()
            val txtView: TextView = findViewById(R.id.txtView)
            txtView.text = getString(R.string.logged_out)
        }

        findViewById<CheckBox>(R.id.checkboxKeys).setOnCheckedChangeListener { _, isChecked ->
            wantsKeys = isChecked
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        account?.close()
    }

    private fun openTab(url: String) {
        val customTabsIntent = CustomTabsIntent.Builder()
                .addDefaultShareMenuItem()
                .setShowTitle(true)
                .build()

        customTabsIntent.intent.data = Uri.parse(url)
        customTabsIntent.launchUrl(this@MainActivity, Uri.parse(url))
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

            val handleAuth = { _: OAuthInfo -> account?.getProfile() }
            val handleProfile = { value: Profile ->
                runOnUiThread {
                    txtView.text = getString(R.string.signed_in, "${value.displayName ?: ""} ${value.email}")
                }
                account?.toJSONString().let {
                    getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE).edit()
                            .putString(FXA_STATE_KEY, it).apply()
                }
            }
            account?.completeOAuthFlow(code, state)?.then(handleAuth)?.whenComplete(handleProfile)
        }
    }

    private fun openWebView(url: String) {
        supportFragmentManager?.beginTransaction()?.apply {
            replace(R.id.container, LoginFragment.create(url, REDIRECT_URL))
            addToBackStack(null)
            commit()
        }
    }

    override fun onLoginComplete(code: String, state: String, fragment: LoginFragment) {
        val txtView: TextView = findViewById(R.id.txtView)
        val handleAuth = { _: OAuthInfo -> account?.getProfile() }
        val handleProfile = { value: Profile ->
            runOnUiThread {
                txtView.text = getString(R.string.signed_in, "${value.displayName ?: ""} ${value.email}")
            }
        }

        account?.completeOAuthFlow(code, state)?.then(handleAuth)?.whenComplete(handleProfile)
        supportFragmentManager?.popBackStack()
    }
}
