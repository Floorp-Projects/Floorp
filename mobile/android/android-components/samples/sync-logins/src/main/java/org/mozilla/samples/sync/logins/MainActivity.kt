/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync.logins

import android.os.Bundle
import android.view.View
import android.widget.ArrayAdapter
import android.widget.ListView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.service.fxa.toAuthType
import mozilla.components.service.sync.logins.AsyncLoginsStorageAdapter
import mozilla.components.service.sync.logins.SyncableLoginsStore
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.rustlog.RustLog
import java.io.File
import kotlin.coroutines.CoroutineContext

const val CLIENT_ID = "3c49430b43dfba77"
const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/$CLIENT_ID"

class MainActivity : AppCompatActivity(), LoginFragment.OnLoginCompleteListener, CoroutineScope, SyncStatusObserver {
    private lateinit var loginsStorage: SyncableLoginsStore
    private lateinit var listView: ListView
    private lateinit var adapter: ArrayAdapter<String>
    private lateinit var activityContext: MainActivity
    private lateinit var account: FirefoxAccount
    private val accountManager by lazy {
        FxaAccountManager(
                applicationContext,
                ServerConfig.release(CLIENT_ID, REDIRECT_URL),
                DeviceConfig("A-C Logins Sync Sample", DeviceType.MOBILE, setOf()),
                SyncConfig(setOf(SyncEngine.Passwords))
        )
    }

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
    get() = Dispatchers.Main + job

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        RustLog.enable()
        RustHttpConfig.setClient(lazy { HttpURLConnectionClient() })

        Log.addSink(AndroidLogSink())

        setContentView(R.layout.activity_main)
        job = Job()

        // Observe sync state changes.
        accountManager.registerForSyncEvents(observer = this, owner = this, autoPause = true)

        listView = findViewById(R.id.logins_list_view)
        adapter = ArrayAdapter(this, android.R.layout.simple_list_item_1)
        listView.adapter = adapter
        activityContext = this

        loginsStorage = SyncableLoginsStore(
            AsyncLoginsStorageAdapter.forDatabase(File(activityContext.filesDir, "logins.sqlite").canonicalPath)
        ) {
            CompletableDeferred("my-not-so-secret-password")
        }
        GlobalSyncableStoreProvider.configureStore(SyncEngine.Passwords to loginsStorage)

        accountManager.register(accountObserver, owner = this, autoPause = true)

        launch { accountManager.initAsync().await() }

        findViewById<View>(R.id.buttonWebView).setOnClickListener {
            launch {
                val authUrl = accountManager.beginAuthenticationAsync().await()
                if (authUrl == null) {
                    Toast.makeText(this@MainActivity, "Account auth error", Toast.LENGTH_LONG).show()
                    return@launch
                }
                openWebView(authUrl)
            }
        }
    }

    private val accountObserver = object : AccountObserver {

        @Suppress("EmptyFunctionBlock")
        override fun onLoggedOut() {}

        override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
            accountManager.syncNowAsync(SyncReason.User)
        }

        @Suppress("EmptyFunctionBlock")
        override fun onProfileUpdated(profile: Profile) {}

        override fun onAuthenticationProblems() {
            launch {
                Toast.makeText(this@MainActivity, "Account auth problem", Toast.LENGTH_LONG).show()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        account.close()
        job.cancel()
    }

    private fun openWebView(url: String) {
        supportFragmentManager.beginTransaction().apply {
            replace(R.id.container, LoginFragment.create(url, REDIRECT_URL))
            addToBackStack(null)
            commit()
        }
    }

    override fun onLoginComplete(code: String, state: String, action: String, fragment: LoginFragment) {
        launch {
            accountManager.finishAuthenticationAsync(
                FxaAuthData(action.toAuthType(), code = code, state = state)
            ).await()
            supportFragmentManager.popBackStack()
        }
    }

    // SyncManager observable interface:
    override fun onStarted() {
        Toast.makeText(this@MainActivity, "Syncing...", Toast.LENGTH_SHORT).show()
    }

    override fun onIdle() {
        Toast.makeText(this@MainActivity, "Logins sync success", Toast.LENGTH_SHORT).show()

        launch {
            val syncedLogins = CoroutineScope(Dispatchers.IO + job).async {
                loginsStorage.withUnlocked {
                    it.list().await()
                }
            }

            adapter.addAll(syncedLogins.await().map { "Login: " + it.hostname })
            adapter.notifyDataSetChanged()
        }
    }

    override fun onError(error: java.lang.Exception?) {
        Toast.makeText(
            this@MainActivity,
            "Logins sync error ${error?.localizedMessage}",
            Toast.LENGTH_SHORT
        ).show()
    }
}
