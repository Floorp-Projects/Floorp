/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync

import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.view.View
import android.widget.TextView
import android.widget.Toast
import kotlinx.android.synthetic.main.activity_main.syncStatus
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventOutgoing
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.support.base.log.Log
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.rustlog.RustLog
import java.lang.Exception
import kotlin.coroutines.CoroutineContext

class MainActivity :
        AppCompatActivity(),
        LoginFragment.OnLoginCompleteListener,
        DeviceFragment.OnDeviceListInteractionListener,
        CoroutineScope {
    private val historyStorage by lazy {
        PlacesHistoryStorage(this)
    }

    private val bookmarksStorage by lazy {
        PlacesBookmarksStorage(this)
    }

    init {
        GlobalSyncableStoreProvider.configureStore("history" to historyStorage)
        GlobalSyncableStoreProvider.configureStore("bookmarks" to bookmarksStorage)
    }

    private val accountManager by lazy {
        FxaAccountManager(
                this,
                ServerConfig.release(CLIENT_ID, REDIRECT_URL),
                DeviceConfig(
                    name = "A-C Sync Sample - ${System.currentTimeMillis()}",
                    type = DeviceType.MOBILE,
                    capabilities = setOf(DeviceCapability.SEND_TAB)
                ),
                SyncConfig(setOf("history", "bookmarks"), syncPeriodInMinutes = 15L)
        )
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
        RustLog.enable()
        RustHttpConfig.setClient(lazy { HttpURLConnectionClient() })

        Log.addSink(AndroidLogSink())

        setContentView(R.layout.activity_main)

        findViewById<View>(R.id.buttonSignIn).setOnClickListener {
            launch {
                val authUrl = accountManager.beginAuthenticationAsync().await()
                if (authUrl == null) {
                    val txtView: TextView = findViewById(R.id.fxaStatusView)
                    txtView.text = getString(R.string.account_error, null)
                    return@launch
                }
                openWebView(authUrl)
            }
        }

        findViewById<View>(R.id.buttonLogout).setOnClickListener {
            launch {
                accountManager.logoutAsync().await()
            }
        }

        findViewById<View>(R.id.refreshDevice).setOnClickListener {
            launch { accountManager.authenticatedAccount()?.deviceConstellation()?.refreshDevicesAsync()?.await() }
        }

        findViewById<View>(R.id.sendTab).setOnClickListener {
            launch {
                accountManager.authenticatedAccount()?.deviceConstellation()?.let { constellation ->
                    // Ignore devices that can't receive tabs.
                    val targets = constellation.state()?.otherDevices?.filter {
                        it.capabilities.contains(DeviceCapability.SEND_TAB)
                    }

                    targets?.forEach {
                        constellation.sendEventToDeviceAsync(
                            it.id, DeviceEventOutgoing.SendTab("Sample tab", "https://www.mozilla.org")
                        ).await()
                    }

                    Toast.makeText(
                        this@MainActivity,
                        "Sent sample tab to ${targets?.size ?: 0} device(s)",
                        Toast.LENGTH_SHORT
                    ).show()
                }
            }
        }

        // NB: ObserverRegistry takes care of unregistering this observer when appropriate, and
        // cleaning up any internal references to 'observer' and 'owner'.
        // Observe changes to the account and profile.
        accountManager.register(accountObserver, owner = this, autoPause = true)
        // Observe sync state changes.
        accountManager.registerForSyncEvents(syncObserver, owner = this, autoPause = true)
        // Observe incoming device events.
        accountManager.registerForDeviceEvents(deviceEventsObserver, owner = this, autoPause = true)

        // Now that our account state observer is registered, we can kick off the account manager.
        launch { accountManager.initAsync().await() }

        findViewById<View>(R.id.buttonSync).setOnClickListener {
            accountManager.syncNowAsync()
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

    override fun onDeviceInteraction(item: Device) {
        Toast.makeText(
            this@MainActivity,
            getString(
                R.string.full_device_details,
                item.id, item.displayName, item.deviceType,
                item.subscriptionExpired, item.subscription, item.capabilities, item.lastAccessTime
            ),
            Toast.LENGTH_LONG
        ).show()
    }

    private fun openWebView(url: String) {
        supportFragmentManager?.beginTransaction()?.apply {
            replace(R.id.container, LoginFragment.create(url, REDIRECT_URL))
            addToBackStack(null)
            commit()
        }
    }

    private val deviceConstellationObserver = object : DeviceConstellationObserver {
        override fun onDevicesUpdate(constellation: ConstellationState) {
            val currentDevice = constellation.currentDevice

            val currentDeviceView: TextView = findViewById(R.id.currentDevice)
            if (currentDevice != null) {
                currentDeviceView.text = getString(
                    R.string.full_device_details,
                    currentDevice.id, currentDevice.displayName, currentDevice.deviceType,
                    currentDevice.subscriptionExpired, currentDevice.subscription,
                    currentDevice.capabilities, currentDevice.lastAccessTime
                )
            } else {
                currentDeviceView.text = getString(R.string.current_device_unknown)
            }

            val devicesFragment = supportFragmentManager.findFragmentById(R.id.devices_fragment) as DeviceFragment
            devicesFragment.updateDevices(constellation.otherDevices)

            Toast.makeText(this@MainActivity, "Devices updated", Toast.LENGTH_SHORT).show()
        }
    }

    private val deviceEventsObserver = object : DeviceEventsObserver {
        override fun onEvents(events: List<DeviceEvent>) {
            val txtView: TextView = findViewById(R.id.latestTabs)
            var tabsStringified = ""
            events.filter { it is DeviceEvent.TabReceived }.forEach {
                val tabReceivedEvent = it as DeviceEvent.TabReceived
                tabsStringified += "Tab(s) from: ${tabReceivedEvent.from?.displayName}\n"
                tabReceivedEvent.entries.forEach { tab ->
                    tabsStringified += "${tab.title}: ${tab.url}\n"
                }
            }
            txtView.text = tabsStringified
        }
    }

    private val accountObserver = object : AccountObserver {
        override fun onLoggedOut() {
            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(R.string.logged_out)

                val historyResultTextView: TextView = findViewById(R.id.historySyncResult)
                historyResultTextView.text = ""
                val bookmarksResultTextView: TextView = findViewById(R.id.bookmarksSyncResult)
                bookmarksResultTextView.text = ""
                val currentDeviceTextView: TextView = findViewById(R.id.currentDevice)
                currentDeviceTextView.text = ""

                val devicesFragment = supportFragmentManager.findFragmentById(
                    R.id.devices_fragment
                ) as DeviceFragment
                devicesFragment.updateDevices(listOf())

                findViewById<View>(R.id.buttonLogout).visibility = View.INVISIBLE
                findViewById<View>(R.id.buttonSignIn).visibility = View.VISIBLE
                findViewById<View>(R.id.buttonSync).visibility = View.INVISIBLE
                findViewById<View>(R.id.refreshDevice).visibility = View.INVISIBLE
                findViewById<View>(R.id.sendTab).visibility = View.INVISIBLE
            }
        }

        override fun onAuthenticationProblems() {
            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(R.string.need_reauth)

                findViewById<View>(R.id.buttonSignIn).visibility = View.VISIBLE
            }
        }

        override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(R.string.signed_in_waiting_for_profile)

                findViewById<View>(R.id.buttonLogout).visibility = View.VISIBLE
                findViewById<View>(R.id.buttonSignIn).visibility = View.INVISIBLE
                findViewById<View>(R.id.buttonSync).visibility = View.VISIBLE
                findViewById<View>(R.id.refreshDevice).visibility = View.VISIBLE
                findViewById<View>(R.id.sendTab).visibility = View.VISIBLE

                account.deviceConstellation().registerDeviceObserver(
                    deviceConstellationObserver,
                    this@MainActivity,
                    true
                )
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
    }

    private val syncObserver = object : SyncStatusObserver {
        override fun onStarted() {
            CoroutineScope(Dispatchers.Main).launch {
                syncStatus?.text = getString(R.string.syncing)
            }
        }

        override fun onIdle() {
            CoroutineScope(Dispatchers.Main).launch {
                syncStatus?.text = getString(R.string.sync_idle)

                val historyResultTextView: TextView = findViewById(R.id.historySyncResult)
                val visitedCount = historyStorage.getVisited().size
                // visitedCount is passed twice: to get the correct plural form, and then as
                // an argument for string formatting.
                historyResultTextView.text = resources.getQuantityString(
                    R.plurals.visited_url_count, visitedCount, visitedCount
                )

                val bookmarksResultTextView: TextView = findViewById(R.id.bookmarksSyncResult)
                val bookmarksRoot = bookmarksStorage.getTree("root________", recursive = true)
                if (bookmarksRoot == null) {
                    bookmarksResultTextView.text = getString(R.string.no_bookmarks_root)
                } else {
                    var bookmarksRootAndChildren = "Bookmarks\n"
                    fun addTreeNode(node: BookmarkNode, depth: Int) {
                        val desc = " ".repeat(depth * 2) + "${node.title} - ${node.url} (${node.guid})\n"
                        bookmarksRootAndChildren += desc
                        node.children?.forEach {
                            addTreeNode(it, depth + 1)
                        }
                    }
                    addTreeNode(bookmarksRoot, 0)
                    bookmarksResultTextView.setHorizontallyScrolling(true)
                    bookmarksResultTextView.setMovementMethod(ScrollingMovementMethod.getInstance())
                    bookmarksResultTextView.text = bookmarksRootAndChildren
                }
            }
        }

        override fun onError(error: Exception?) {
            CoroutineScope(Dispatchers.Main).launch {
                syncStatus?.text = getString(R.string.sync_error, error)
            }
        }
    }
}
