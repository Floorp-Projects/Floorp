/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.fxa

import android.content.Context
import android.net.Uri
import android.os.Bundle
import androidx.browser.customtabs.CustomTabsIntent
import android.view.View
import android.content.Intent
import android.widget.CheckBox
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.Profile
import mozilla.components.feature.qr.QrFeature
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FxaException
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.rustlog.RustLog
import kotlin.coroutines.CoroutineContext

@Suppress("TooManyFunctions")
open class MainActivity : AppCompatActivity(), LoginFragment.OnLoginCompleteListener, CoroutineScope {
    private lateinit var account: FirefoxAccount
    private var scopesWithoutKeys: Set<String> = setOf("profile")
    private var scopesWithKeys: Set<String> = setOf("profile", "https://identity.mozilla.com/apps/oldsync")
    private var scopes: Set<String> = scopesWithoutKeys
    private var wantsKeys: Boolean = false

    private lateinit var qrFeature: QrFeature

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    companion object {
        const val CLIENT_ID = "3c49430b43dfba77"
        const val CONFIG_URL = "https://accounts.firefox.com"
        const val REDIRECT_URL = "$CONFIG_URL/oauth/success/3c49430b43dfba77"
        const val FXA_STATE_PREFS_KEY = "fxaAppState"
        const val FXA_STATE_KEY = "fxaState"
        private const val REQUEST_CODE_CAMERA_PERMISSIONS = 1
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        RustLog.enable()
        RustHttpConfig.setClient(lazy { HttpURLConnectionClient() })

        Log.addSink(AndroidLogSink())

        setContentView(R.layout.activity_main)
        job = Job()
        account = initAccount()

        qrFeature = QrFeature(
            this,
            fragmentManager = supportFragmentManager,
            onNeedToRequestPermissions = { permissions ->
                ActivityCompat.requestPermissions(this, permissions, REQUEST_CODE_CAMERA_PERMISSIONS)
            },
            onScanResult = { pairingUrl ->
                launch {
                    val url = account.beginPairingFlowAsync(pairingUrl, scopes).await()
                    if (url == null) {
                        Log.log(
                            Log.Priority.ERROR,
                            tag = "mozac-samples-fxa",
                            message = "Pairing flow failed for $pairingUrl"
                        )
                        return@launch
                    }
                    openWebView(url)
                }
            }
        )

        lifecycle.addObserver(qrFeature)

        findViewById<View>(R.id.buttonCustomTabs).setOnClickListener {
            launch {
                account.beginOAuthFlowAsync(scopes, wantsKeys).await()?.let {
                    openTab(it)
                }
            }
        }

        findViewById<View>(R.id.buttonWebView).setOnClickListener {
            launch {
                account.beginOAuthFlowAsync(scopes, wantsKeys).await()?.let {
                    openWebView(it)
                }
            }
        }

        findViewById<View>(R.id.buttonPair).setOnClickListener {
            qrFeature.scan()
        }

        findViewById<View>(R.id.buttonLogout).setOnClickListener {
            getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE).edit().putString(FXA_STATE_KEY, "").apply()
            val txtView: TextView = findViewById(R.id.txtView)
            txtView.text = getString(R.string.logged_out)
        }

        findViewById<CheckBox>(R.id.checkboxKeys).setOnCheckedChangeListener { _, isChecked ->
            wantsKeys = isChecked
            scopes = if (isChecked) scopesWithKeys else scopesWithoutKeys
        }
    }

    private fun initAccount(): FirefoxAccount {
        getAuthenticatedAccount()?.let {
            launch {
                it.getProfileAsync(true).await()?.let {
                    displayProfile(it)
                }
            }
            return it
        }

        val config = ServerConfig(CONFIG_URL, CLIENT_ID, REDIRECT_URL)
        return FirefoxAccount(config)
    }

    override fun onDestroy() {
        super.onDestroy()
        account.close()
        job.cancel()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        val action = intent.action
        val data = intent.dataString

        if (Intent.ACTION_VIEW == action && data != null) {
            val url = Uri.parse(data)
            val code = url.getQueryParameter("code")!!
            val state = url.getQueryParameter("state")!!
            displayAndPersistProfile(code, state)
        }
    }

    override fun onLoginComplete(code: String, state: String, fragment: LoginFragment) {
        displayAndPersistProfile(code, state)
        supportFragmentManager?.popBackStack()
    }

    private fun getAuthenticatedAccount(): FirefoxAccount? {
        val savedJSON = getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE).getString(FXA_STATE_KEY, "")
        return savedJSON?.let {
            try {
                FirefoxAccount.fromJSONString(it)
            } catch (e: FxaException) {
                null
            }
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

    private fun openWebView(url: String) {
        supportFragmentManager?.beginTransaction()?.apply {
            replace(R.id.container, LoginFragment.create(url, REDIRECT_URL))
            addToBackStack(null)
            commit()
        }
    }

    private fun displayAndPersistProfile(code: String, state: String) {
        launch {
            account.completeOAuthFlowAsync(code, state).await()
            account.getProfileAsync().await()?.let {
                displayProfile(it)
            }
            account.toJSONString().let {
                getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE)
                        .edit().putString(FXA_STATE_KEY, it).apply()
            }
        }
    }

    private fun displayProfile(profile: Profile) {
        val txtView: TextView = findViewById(R.id.txtView)
        txtView.text = getString(R.string.signed_in, "${profile.displayName ?: ""} ${profile.email}")
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        when (requestCode) {
            REQUEST_CODE_CAMERA_PERMISSIONS -> qrFeature.onPermissionsResult(permissions, grantResults)
        }
    }

    override fun onBackPressed() {
        if (!qrFeature.onBackPressed()) {
            super.onBackPressed()
        }
    }
}
