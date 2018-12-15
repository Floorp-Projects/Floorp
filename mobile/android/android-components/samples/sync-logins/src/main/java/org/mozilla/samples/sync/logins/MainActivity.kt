/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync.logins

import android.content.Context
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.View
import android.widget.Toast
import android.widget.ArrayAdapter
import android.widget.ListView
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.SyncError
import mozilla.components.feature.sync.FirefoxSyncFeature
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FxaException
import mozilla.components.service.sync.logins.AsyncLoginsStorageAdapter
import mozilla.components.service.sync.logins.SyncableLoginsStore
import mozilla.components.service.sync.logins.SyncUnlockInfo
import java.io.File
import kotlin.coroutines.CoroutineContext

open class MainActivity : AppCompatActivity(), LoginFragment.OnLoginCompleteListener, CoroutineScope {
    private var scopes: Array<String> = arrayOf("profile", "https://identity.mozilla.com/apps/oldsync")
    private var wantsKeys: Boolean = true

    private val loginsStoreName: String = "placesLogins"
    private val loginsStore by lazy {
        SyncableLoginsStore(
            AsyncLoginsStorageAdapter.forDatabase(File(applicationContext.filesDir, "logins.sqlite").canonicalPath)
        ) {
            CompletableDeferred("my-not-so-secret-password")
        }
    }

    private val featureSync by lazy {
        FirefoxSyncFeature(mapOf(Pair(loginsStoreName, loginsStore))) { authInfo ->
            SyncUnlockInfo(
                fxaAccessToken = authInfo.fxaAccessToken,
                kid = authInfo.kid,
                syncKey = authInfo.syncKey,
                tokenserverURL = authInfo.tokenServerUrl
            )
        }
    }

    private lateinit var listView: ListView
    private lateinit var adapter: ArrayAdapter<String>
    private lateinit var activityContext: MainActivity
    private lateinit var whenAccount: Deferred<FirefoxAccount>

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    companion object {
        const val CLIENT_ID = "3c49430b43dfba77"
        const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/3c49430b43dfba77"
        const val FXA_STATE_PREFS_KEY = "fxaAppState"
        const val FXA_STATE_KEY = "fxaState"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        job = Job()

        listView = findViewById(R.id.logins_list_view)
        adapter = ArrayAdapter(this, android.R.layout.simple_list_item_1)
        listView.adapter = adapter
        activityContext = this

        whenAccount = async {
            getAuthenticatedAccount()?.let {
                return@async it
            }

            FirefoxAccount(Config.release(CLIENT_ID, REDIRECT_URL))
        }

        findViewById<View>(R.id.buttonWebView).setOnClickListener {
            launch {
                val url = whenAccount.await().beginOAuthFlow(scopes, wantsKeys).await()
                openWebView(url)
            }
        }
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

    override fun onDestroy() {
        super.onDestroy()
        runBlocking { whenAccount.await().close() }
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
            val account = whenAccount.await()

            account.completeOAuthFlow(code, state).await()
            account.toJSONString().let {
                getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE)
                        .edit().putString(FXA_STATE_KEY, it).apply()
            }

            syncLogins(account)

            supportFragmentManager?.popBackStack()
        }
    }

    private suspend fun syncLogins(account: FirefoxAccount) {
        val syncResult = CoroutineScope(Dispatchers.IO + job).async {
            featureSync.sync(account)
        }.await()

        check(loginsStoreName in syncResult) { "Expected to synchronize a logins store" }

        val loginsSyncStatus = syncResult[loginsStoreName]!!.status
        if (loginsSyncStatus is SyncError) {
            Toast.makeText(
                this@MainActivity,
                "Logins sync error: " + loginsSyncStatus.exception.localizedMessage,
                Toast.LENGTH_SHORT
            ).show()
        } else {
            Toast.makeText(this@MainActivity, "Logins sync success", Toast.LENGTH_SHORT).show()
        }

        val syncedLogins = loginsStore.withUnlocked {
            it.list().await()
        }

        adapter.addAll(syncedLogins.map { "Login: " + it.hostname })
        adapter.notifyDataSetChanged()
    }
}
