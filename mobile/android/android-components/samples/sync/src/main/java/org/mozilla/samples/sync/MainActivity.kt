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
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.lib.dataprotect.generateEncryptionKey
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.support.base.log.Log
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.service.fxa.toAuthType
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.rustlog.RustLog
import java.lang.Exception
import kotlin.coroutines.CoroutineContext

private const val PASSWORDS_ENCRYPTION_KEY_STRENGTH = 256

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

    private val securePreferences by lazy { SecureAbove22Preferences(this, "key_store") }

    private val passwordsEncryptionKey by lazy {
        securePreferences.getString(SyncEngine.Passwords.nativeName)
            ?: generateEncryptionKey(PASSWORDS_ENCRYPTION_KEY_STRENGTH).also {
                securePreferences.putString(SyncEngine.Passwords.nativeName, it)
            }
    }

    private val passwordsStorage by lazy { SyncableLoginsStorage(this, passwordsEncryptionKey) }

    private val accountManager by lazy {
        FxaAccountManager(
                this,
                ServerConfig.release(CLIENT_ID, REDIRECT_URL),
                DeviceConfig(
                    name = "A-C Sync Sample - ${System.currentTimeMillis()}",
                    type = DeviceType.MOBILE,
                    capabilities = setOf(DeviceCapability.SEND_TAB),
                    secureStateAtRest = true
                ),
                SyncConfig(
                    setOf(SyncEngine.History, SyncEngine.Bookmarks, SyncEngine.Passwords),
                    syncPeriodInMinutes = 15L
                )
        )
    }

    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    companion object {
        const val CLIENT_ID = "3c49430b43dfba77"
        const val REDIRECT_URL = "https://accounts.firefox.com/oauth/success/$CLIENT_ID"
    }

    private val logger = Logger("SampleSync")

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
                        constellation.sendCommandToDeviceAsync(
                            it.id, DeviceCommandOutgoing.SendTab("Sample tab", "https://www.mozilla.org")
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
        // Observe incoming device commands.
        accountManager.registerForAccountEvents(accountEventsObserver, owner = this, autoPause = true)

        launch {
            GlobalSyncableStoreProvider.configureStore(SyncEngine.History to historyStorage)
            GlobalSyncableStoreProvider.configureStore(SyncEngine.Bookmarks to bookmarksStorage)
            GlobalSyncableStoreProvider.configureStore(SyncEngine.Passwords to passwordsStorage)

            // Now that our account state observer is registered, we can kick off the account manager.
            accountManager.initAsync().await()
        }

        findViewById<View>(R.id.buttonSync).setOnClickListener {
            accountManager.syncNowAsync(SyncReason.User)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        accountManager.close()
        job.cancel()
    }

    override fun onLoginComplete(code: String, state: String, action: String, fragment: LoginFragment) {
        launch {
            supportFragmentManager.popBackStack()
            accountManager.finishAuthenticationAsync(
                FxaAuthData(action.toAuthType(), code = code, state = state)
            ).await()
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
        supportFragmentManager.beginTransaction().apply {
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

    @Suppress("SetTextI18n", "NestedBlockDepth")
    private val accountEventsObserver = object : AccountEventsObserver {
        override fun onEvents(events: List<AccountEvent>) {
            val txtView: TextView = findViewById(R.id.latestTabs)
            events.forEach {
                when (it) {
                    is AccountEvent.DeviceCommandIncoming -> {
                        when (it.command) {
                            is DeviceCommandIncoming.TabReceived -> {
                                val cmd = it.command as DeviceCommandIncoming.TabReceived
                                txtView.text = "A tab was received"
                                var tabsStringified = "Tab(s) from: ${cmd.from?.displayName}\n"
                                cmd.entries.forEach { tab ->
                                    tabsStringified += "${tab.title}: ${tab.url}\n"
                                }
                                txtView.text = tabsStringified
                            }
                        }
                    }
                    is AccountEvent.ProfileUpdated -> {
                        txtView.text = "The user's profile was updated"
                    }
                    is AccountEvent.AccountAuthStateChanged -> {
                        txtView.text = "The account auth state changed"
                    }
                    is AccountEvent.AccountDestroyed -> {
                        txtView.text = "The account was destroyed"
                    }
                    is AccountEvent.DeviceConnected -> {
                        txtView.text = "Another device connected to the account"
                    }
                    is AccountEvent.DeviceDisconnected -> {
                        if (it.isLocalDevice) {
                            txtView.text = "This device disconnected"
                        } else {
                            txtView.text = "The device ${it.deviceId} disconnected"
                        }
                    }
                }
            }
        }
    }

    private val accountObserver = object : AccountObserver {
        lateinit var lastAuthType: AuthType

        override fun onLoggedOut() {
            logger.info("onLoggedOut")

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
            logger.info("onAuthenticationProblems")

            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(R.string.need_reauth)

                findViewById<View>(R.id.buttonSignIn).visibility = View.VISIBLE
            }
        }

        override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
            logger.info("onAuthenticated")

            launch {
                lastAuthType = authType

                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(R.string.signed_in_waiting_for_profile, authType::class.simpleName)

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
            logger.info("onProfileUpdated")

            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(
                    R.string.signed_in_with_profile,
                    lastAuthType::class.simpleName,
                    "${profile.displayName ?: ""} ${profile.email}"
                )
            }
        }
    }

    private val syncObserver = object : SyncStatusObserver {
        override fun onStarted() {
            logger.info("onSyncStarted")
            CoroutineScope(Dispatchers.Main).launch {
                syncStatus?.text = getString(R.string.syncing)
            }
        }

        override fun onIdle() {
            logger.info("onSyncIdle")
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
                    var bookmarksRootAndChildren = "BOOKMARKS\n"
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
            logger.error("onSyncError", error)
            CoroutineScope(Dispatchers.Main).launch {
                syncStatus?.text = getString(R.string.sync_error, error)
            }
        }
    }
}
