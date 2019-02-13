/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync.history

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.View
import android.widget.TextView
import kotlinx.android.synthetic.main.activity_main.historySyncStatus
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.browser.storage.sync.SyncAuthInfo
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.SyncError
import mozilla.components.concept.sync.SyncStatusObserver
import mozilla.components.feature.sync.FirefoxSyncFeature
import mozilla.components.service.fxa.FxaAccountManager
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FxaException
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import java.lang.Exception
import kotlin.coroutines.CoroutineContext

class MainActivity : AppCompatActivity(), LoginFragment.OnLoginCompleteListener, CoroutineScope {

    private val historyStoreName = "placesHistory"
    private val historyStorage by lazy { PlacesHistoryStorage(applicationContext) }

    private val accountManager by lazy {
        FxaAccountManager(
            this,
            Config.release(CLIENT_ID, REDIRECT_URL),
            arrayOf("profile", "https://identity.mozilla.com/apps/oldsync")
        )
    }
    private val featureSync by lazy {
        FirefoxSyncFeature(
            syncableStores = mapOf(historyStoreName to historyStorage),
            syncScope = "https://identity.mozilla.com/apps/oldsync"
        ) { authInfo ->
            SyncAuthInfo(
                fxaAccessToken = authInfo.fxaAccessToken,
                kid = authInfo.kid,
                syncKey = authInfo.syncKey,
                tokenserverURL = authInfo.tokenServerUrl
            )
        }
    }

    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    companion object {
        const val CLIENT_ID = "3c49430b43dfba77"
        const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/$CLIENT_ID"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        Log.addSink(AndroidLogSink())

        setContentView(R.layout.activity_main)

        findViewById<View>(R.id.buttonSignIn).setOnClickListener {
            launch {
                val authUrl = try {
                    accountManager.beginAuthenticationAsync().await()
                } catch (error: FxaException) {
                    val txtView: TextView = findViewById(R.id.fxaStatusView)
                    txtView.text = getString(R.string.account_error, error.toString())
                    return@launch
                }
                openWebView(authUrl)
            }
        }

        findViewById<View>(R.id.buttonLogout).setOnClickListener {
            launch { accountManager.logoutAsync().await() }
        }

        // NB: ObserverRegistry takes care of unregistering this observer when appropriate, and
        // cleaning up any internal references to 'observer' and 'owner'.
        featureSync.register(syncObserver, owner = this, autoPause = true)
        // Observe changes to the account and profile.
        accountManager.register(accountObserver, owner = this, autoPause = true)

        // Now that our account state observer is registered, we can kick off the account manager.
        launch { accountManager.initAsync().await() }

        findViewById<View>(R.id.buttonSyncHistory).setOnClickListener {
            val account = accountManager.authenticatedAccount() ?: return@setOnClickListener

            val txtView: TextView = findViewById(R.id.historySyncResult)

            launch {
                val syncResult = CoroutineScope(Dispatchers.IO + job).async {
                    featureSync.sync(account)
                }.await()

                check(historyStoreName in syncResult) { "Expected to synchronize a history store" }

                val historySyncStatus = syncResult[historyStoreName]!!.status
                if (historySyncStatus is SyncError) {
                    txtView.text = getString(R.string.sync_error, historySyncStatus.exception)
                } else {
                    val visitedCount = historyStorage.getVisited().size
                    // visitedCount is passed twice: to get the correct plural form, and then as
                    // an argument for string formatting.
                    txtView.text = resources.getQuantityString(
                        R.plurals.visited_url_count, visitedCount, visitedCount
                    )
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        accountManager.close()
        job.cancel()
    }

    override fun onLoginComplete(code: String, state: String, fragment: LoginFragment) {
        launch {
            supportFragmentManager?.popBackStack()
            accountManager.finishAuthenticationAsync(code, state).await()
        }
    }

    private fun openWebView(url: String) {
        supportFragmentManager?.beginTransaction()?.apply {
            replace(R.id.container, LoginFragment.create(url, REDIRECT_URL))
            addToBackStack(null)
            commit()
        }
    }

    private val accountObserver = object : AccountObserver {
        override fun onLoggedOut() {
            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(R.string.logged_out)

                findViewById<View>(R.id.buttonLogout).visibility = View.INVISIBLE
                findViewById<View>(R.id.buttonSignIn).visibility = View.VISIBLE
                findViewById<View>(R.id.buttonSyncHistory).visibility = View.INVISIBLE
            }
        }

        override fun onAuthenticated(account: OAuthAccount) {
            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(R.string.signed_in_waiting_for_profile)

                findViewById<View>(R.id.buttonLogout).visibility = View.VISIBLE
                findViewById<View>(R.id.buttonSignIn).visibility = View.INVISIBLE
                findViewById<View>(R.id.buttonSyncHistory).visibility = View.VISIBLE
            }
        }

        override fun onProfileUpdated(profile: Profile) {
            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(
                    R.string.signed_in_with_profile,
                    "${profile.displayName ?: ""} ${profile.email}"
                )
            }
        }

        override fun onError(error: Exception) {
            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(R.string.account_error, error.toString())
            }
        }
    }

    private val syncObserver = object : SyncStatusObserver {
        override fun onStarted() {
            CoroutineScope(Dispatchers.Main).launch {
                historySyncStatus?.text = getString(R.string.syncing)
            }
        }

        override fun onIdle() {
            CoroutineScope(Dispatchers.Main).launch {
                historySyncStatus?.text = getString(R.string.sync_idle)
            }
        }

        override fun onError(error: Exception?) {
            CoroutineScope(Dispatchers.Main).launch {
                historySyncStatus?.text = getString(R.string.sync_error, error)
            }
        }
    }
}
