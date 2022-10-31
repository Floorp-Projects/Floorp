/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync

import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.view.View
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthFlowError
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DeviceConfig
import mozilla.components.concept.sync.DeviceConstellationObserver
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.PeriodicSyncConfig
import mozilla.components.service.fxa.Server
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.GlobalSyncableStoreProvider
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.service.fxa.toAuthType
import mozilla.components.service.sync.autofill.AutofillCreditCardsAddressesStorage
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.rusthttp.RustHttpConfig
import mozilla.components.support.rustlog.RustLog
import org.mozilla.samples.sync.databinding.ActivityMainBinding
import java.lang.Exception
import kotlin.coroutines.CoroutineContext

class MainActivity :
    AppCompatActivity(),
    LoginFragment.OnLoginCompleteListener,
    DeviceFragment.OnDeviceListInteractionListener,
    CoroutineScope {
    private val historyStorage = lazy {
        PlacesHistoryStorage(this)
    }

    private val bookmarksStorage = lazy {
        PlacesBookmarksStorage(this)
    }

    private val securePreferences by lazy { SecureAbove22Preferences(this, "key_store") }

    private val passwordsStorage = lazy {
        SyncableLoginsStorage(this, lazy { securePreferences })
    }

    private val creditCardsAddressesStorage = lazy {
        AutofillCreditCardsAddressesStorage(this, lazy { securePreferences })
    }

    private val creditCardKeyProvider by lazy { creditCardsAddressesStorage.value.crypto }
    private val passwordsKeyProvider by lazy { passwordsStorage.value.crypto }

    private val accountManager by lazy {
        FxaAccountManager(
            this,
            ServerConfig(Server.RELEASE, CLIENT_ID, REDIRECT_URL),
            DeviceConfig(
                name = "A-C Sync Sample - ${System.currentTimeMillis()}",
                type = DeviceType.MOBILE,
                capabilities = setOf(DeviceCapability.SEND_TAB),
                secureStateAtRest = true,
            ),
            SyncConfig(
                setOf(
                    SyncEngine.History,
                    SyncEngine.Bookmarks,
                    SyncEngine.Passwords,
                    SyncEngine.Addresses,
                    SyncEngine.CreditCards,
                ),
                periodicSyncConfig = PeriodicSyncConfig(periodMinutes = 15, initialDelayMinutes = 5),
            ),
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

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)

        RustLog.enable()
        RustHttpConfig.setClient(lazy { HttpURLConnectionClient() })

        Log.addSink(AndroidLogSink())

        setContentView(binding.root)

        findViewById<View>(R.id.buttonSignIn).setOnClickListener {
            launch {
                accountManager.beginAuthentication()?.let { openWebView(it) }
            }
        }

        findViewById<View>(R.id.buttonLogout).setOnClickListener {
            launch { accountManager.logout() }
        }

        findViewById<View>(R.id.refreshDevice).setOnClickListener {
            launch { accountManager.authenticatedAccount()?.deviceConstellation()?.refreshDevices() }
        }

        findViewById<View>(R.id.sendTab).setOnClickListener {
            launch {
                accountManager.authenticatedAccount()?.deviceConstellation()?.let { constellation ->
                    // Ignore devices that can't receive tabs.
                    val targets = constellation.state()?.otherDevices?.filter {
                        it.capabilities.contains(DeviceCapability.SEND_TAB)
                    }

                    targets?.forEach {
                        constellation.sendCommandToDevice(
                            it.id,
                            DeviceCommandOutgoing.SendTab("Sample tab", "https://www.mozilla.org"),
                        )
                    }

                    Toast.makeText(
                        this@MainActivity,
                        "Sent sample tab to ${targets?.size ?: 0} device(s)",
                        Toast.LENGTH_SHORT,
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

        GlobalSyncableStoreProvider.configureStore(SyncEngine.History to historyStorage)
        GlobalSyncableStoreProvider.configureStore(SyncEngine.Bookmarks to bookmarksStorage)
        GlobalSyncableStoreProvider.configureStore(
            storePair = SyncEngine.Passwords to passwordsStorage,
            keyProvider = lazy { passwordsKeyProvider },
        )
        GlobalSyncableStoreProvider.configureStore(
            storePair = SyncEngine.CreditCards to creditCardsAddressesStorage,
            keyProvider = lazy { creditCardKeyProvider },
        )
        GlobalSyncableStoreProvider.configureStore(SyncEngine.Addresses to creditCardsAddressesStorage)

        launch {
            // Now that our account state observer is registered, we can kick off the account manager.
            accountManager.start()
        }

        findViewById<View>(R.id.buttonSync).setOnClickListener {
            launch {
                accountManager.syncNow(SyncReason.User)
                accountManager.authenticatedAccount()?.deviceConstellation()?.pollForCommands()
            }
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
            accountManager.finishAuthentication(
                FxaAuthData(action.toAuthType(), code = code, state = state),
            )
        }
    }

    override fun onDeviceInteraction(item: Device) {
        Toast.makeText(
            this@MainActivity,
            getString(
                R.string.full_device_details,
                item.id,
                item.displayName,
                item.deviceType,
                item.subscriptionExpired,
                item.subscription,
                item.capabilities,
                item.lastAccessTime,
            ),
            Toast.LENGTH_LONG,
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
            launch {
                val currentDevice = constellation.currentDevice

                val currentDeviceView: TextView = findViewById(R.id.currentDevice)
                if (currentDevice != null) {
                    currentDeviceView.text = getString(
                        R.string.full_device_details,
                        currentDevice.id,
                        currentDevice.displayName,
                        currentDevice.deviceType,
                        currentDevice.subscriptionExpired,
                        currentDevice.subscription,
                        currentDevice.capabilities,
                        currentDevice.lastAccessTime,
                    )
                } else {
                    currentDeviceView.text = getString(R.string.current_device_unknown)
                }

                val devicesFragment = supportFragmentManager.findFragmentById(R.id.devices_fragment) as DeviceFragment
                devicesFragment.updateDevices(constellation.otherDevices)

                Toast.makeText(this@MainActivity, "Devices updated", Toast.LENGTH_SHORT).show()
            }
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
                    R.id.devices_fragment,
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
                    true,
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
                    "${profile.displayName ?: ""} ${profile.email}",
                )
            }
        }

        override fun onFlowError(error: AuthFlowError) {
            launch {
                val txtView: TextView = findViewById(R.id.fxaStatusView)
                txtView.text = getString(
                    R.string.account_error,
                    when (error) {
                        AuthFlowError.FailedToBeginAuth -> "Failed to begin authentication"
                        AuthFlowError.FailedToCompleteAuth -> "Failed to complete authentication"
                        AuthFlowError.FailedToMigrate -> "Failed to migrate"
                    },
                )
            }
        }
    }

    private val syncObserver = object : SyncStatusObserver {
        override fun onStarted() {
            logger.info("onSyncStarted")
            CoroutineScope(Dispatchers.Main).launch {
                binding.syncStatus.text = getString(R.string.syncing)
            }
        }

        override fun onIdle() {
            logger.info("onSyncIdle")
            CoroutineScope(Dispatchers.Main).launch {
                binding.syncStatus.text = getString(R.string.sync_idle)

                val historyResultTextView: TextView = findViewById(R.id.historySyncResult)
                val visitedCount = withContext(Dispatchers.IO) { historyStorage.value.getVisited().size }
                // visitedCount is passed twice: to get the correct plural form, and then as
                // an argument for string formatting.
                historyResultTextView.text = resources.getQuantityString(
                    R.plurals.visited_url_count,
                    visitedCount,
                    visitedCount,
                )

                val bookmarksResultTextView: TextView = findViewById(R.id.bookmarksSyncResult)
                bookmarksResultTextView.setHorizontallyScrolling(true)
                bookmarksResultTextView.movementMethod = ScrollingMovementMethod.getInstance()
                bookmarksResultTextView.text = withContext(Dispatchers.IO) {
                    val bookmarksRoot = bookmarksStorage.value.getTree("root________", recursive = true)
                    if (bookmarksRoot == null) {
                        getString(R.string.no_bookmarks_root)
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
                        bookmarksRootAndChildren
                    }
                }
            }
        }

        override fun onError(error: Exception?) {
            logger.error("onSyncError", error)
            CoroutineScope(Dispatchers.Main).launch {
                binding.syncStatus.text = getString(R.string.sync_error, error)
            }
        }
    }
}
