/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync.logins

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.View
import android.widget.Toast
import android.widget.ArrayAdapter
import android.widget.ListView
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.SyncStatusObserver
import mozilla.components.service.fxa.FxaAccountManager
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FxaException
import mozilla.components.feature.sync.BackgroundSyncManager
import mozilla.components.feature.sync.GlobalSyncableStoreProvider
import mozilla.components.service.sync.logins.AsyncLoginsStorageAdapter
import mozilla.components.service.sync.logins.SyncableLoginsStore
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import java.io.File
import kotlin.coroutines.CoroutineContext

const val CLIENT_ID = "3c49430b43dfba77"
const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/$CLIENT_ID"

class MainActivity : AppCompatActivity(), LoginFragment.OnLoginCompleteListener, CoroutineScope, SyncStatusObserver {
    private val loginsStorage by lazy {
        SyncableLoginsStore(
                AsyncLoginsStorageAdapter.forDatabase(File(this.filesDir, "logins.sqlite").canonicalPath)
        ) {
            CompletableDeferred("my-not-so-secret-password")
        }
    }

    private val syncManager by lazy {
        GlobalSyncableStoreProvider.configureStore("logins" to loginsStorage)
        BackgroundSyncManager("https://identity.mozilla.com/apps/oldsync").also {
            it.addStore("logins")
        }
    }

    private lateinit var listView: ListView
    private lateinit var adapter: ArrayAdapter<String>
    private lateinit var activityContext: MainActivity
    private lateinit var account: FirefoxAccount
    private val accountManager by lazy {
        FxaAccountManager(
            applicationContext,
            Config.release(CLIENT_ID, REDIRECT_URL),
            arrayOf("profile", "https://identity.mozilla.com/apps/oldsync"),
            syncManager
        )
    }

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
    get() = Dispatchers.Main + job

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        Log.addSink(AndroidLogSink())

        setContentView(R.layout.activity_main)
        job = Job()

        syncManager.register(this, this)

        listView = findViewById(R.id.logins_list_view)
        adapter = ArrayAdapter(this, android.R.layout.simple_list_item_1)
        listView.adapter = adapter
        activityContext = this

        accountManager.register(accountObserver, owner = this, autoPause = true)

        launch { accountManager.initAsync().await() }

        findViewById<View>(R.id.buttonWebView).setOnClickListener {
            launch {
                val authUrl = try {
                    accountManager.beginAuthenticationAsync().await()
                } catch (error: FxaException) {
                    Toast.makeText(this@MainActivity, "Account auth error: $error", Toast.LENGTH_LONG).show()
                    return@launch
                }
                openWebView(authUrl)
            }
        }
    }

    private val accountObserver = object : AccountObserver {
        override fun onLoggedOut() {}

        override fun onAuthenticated(account: OAuthAccount) {
            syncManager.syncNow()
        }

        override fun onProfileUpdated(profile: Profile) {}

        override fun onError(error: Exception) {
            launch {
                Toast.makeText(this@MainActivity, "Account error: $error", Toast.LENGTH_LONG).show()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        account.close()
        job.cancel()
    }

    private fun openWebView(url: String) {
        supportFragmentManager?.beginTransaction()?.apply {
            replace(R.id.container, LoginFragment.create(url, REDIRECT_URL))
            addToBackStack(null)
            commit()
        }
    }

    override fun onLoginComplete(code: String, state: String, fragment: LoginFragment) {
        launch {
            accountManager.finishAuthenticationAsync(code, state).await()
            supportFragmentManager?.popBackStack()
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
