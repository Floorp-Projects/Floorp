/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.app.Application
import android.content.Context
import android.content.Intent
import android.content.pm.ResolveInfo
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkRequest
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.getSystemService
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.feature.share.RecentAppsStorage
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.utils.ext.queryIntentActivitiesCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.isOnline
import org.mozilla.fenix.share.DefaultShareController.Companion.ACTION_COPY_LINK_TO_CLIPBOARD
import org.mozilla.fenix.share.listadapters.AppShareOption
import org.mozilla.fenix.share.listadapters.SyncShareOption

class ShareViewModel(application: Application) : AndroidViewModel(application) {

    companion object {
        internal const val RECENT_APPS_LIMIT = 6
    }

    private val connectivityManager by lazy { application.getSystemService<ConnectivityManager>() }
    private val fxaAccountManager = application.components.backgroundServices.accountManager

    @VisibleForTesting
    internal var recentAppsStorage = RecentAppsStorage(application.applicationContext)

    @VisibleForTesting
    internal var ioDispatcher = Dispatchers.IO

    private val devicesListLiveData = MutableLiveData<List<SyncShareOption>>(emptyList())
    private val appsListLiveData = MutableLiveData<List<AppShareOption>>(emptyList())
    private val recentAppsListLiveData = MutableLiveData<List<AppShareOption>>(emptyList())

    @VisibleForTesting
    internal val networkCallback = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network) = reloadDevices(network)
        override fun onAvailable(network: Network) = reloadDevices(network)

        private fun reloadDevices(network: Network?) {
            viewModelScope.launch(ioDispatcher) {
                fxaAccountManager.authenticatedAccount()
                    ?.deviceConstellation()
                    ?.refreshDevices()

                val devicesShareOptions = buildDeviceList(fxaAccountManager, network)
                devicesListLiveData.postValue(devicesShareOptions)
            }
        }
    }

    /**
     * List of devices and sync-related share options.
     */
    val devicesList: LiveData<List<SyncShareOption>> get() = devicesListLiveData

    /**
     * List of applications that can be shared to.
     */
    val appsList: LiveData<List<AppShareOption>> get() = appsListLiveData

    /**
     * List of recent applications that can be shared to.
     */
    val recentAppsList: LiveData<List<AppShareOption>> get() = recentAppsListLiveData

    /**
     * Load a list of devices and apps into [devicesList] and [appsList].
     * Should be called when the fragment is attached so the data can be fetched early.
     */
    fun loadDevicesAndApps(context: Context) {
        val networkRequest = NetworkRequest.Builder().build()
        connectivityManager?.registerNetworkCallback(networkRequest, networkCallback)

        // Start preparing the data as soon as we have a valid Context
        viewModelScope.launch(ioDispatcher) {
            val shareIntent = Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            }
            val shareAppsActivities = getIntentActivities(shareIntent, context)

            var apps = buildAppsList(shareAppsActivities, context)
            recentAppsStorage.updateDatabaseWithNewApps(apps.map { app -> app.activityName })
            val recentApps = buildRecentAppsList(apps)
            apps = filterOutRecentApps(apps, recentApps)

            // if copy app is available, prepend to the list of actions
            getCopyApp(context)?.let {
                apps = listOf(it) + apps
            }

            recentAppsListLiveData.postValue(recentApps)
            appsListLiveData.postValue(apps)
        }

        viewModelScope.launch(ioDispatcher) {
            val devices = buildDeviceList(fxaAccountManager)
            devicesListLiveData.postValue(devices)
        }
    }

    private fun getCopyApp(context: Context): AppShareOption? {
        val copyIcon = AppCompatResources.getDrawable(context, R.drawable.ic_share_clipboard)

        return copyIcon?.let {
            AppShareOption(
                context.getString(R.string.share_copy_link_to_clipboard),
                copyIcon,
                ACTION_COPY_LINK_TO_CLIPBOARD,
                "",
            )
        }
    }

    private fun filterOutRecentApps(
        apps: List<AppShareOption>,
        recentApps: List<AppShareOption>,
    ): List<AppShareOption> {
        return apps.filter { app -> !recentApps.contains(app) }
    }

    @WorkerThread
    internal fun buildRecentAppsList(apps: List<AppShareOption>): List<AppShareOption> {
        val recentAppsDatabase = recentAppsStorage.getRecentAppsUpTo(RECENT_APPS_LIMIT)
        val result: MutableList<AppShareOption> = ArrayList()
        for (recentApp in recentAppsDatabase) {
            for (app in apps) {
                if (recentApp.activityName == app.activityName) {
                    result.add(app)
                }
            }
        }
        return result
    }

    /**
     * Unregisters the network callback and cleans up.
     */
    override fun onCleared() {
        connectivityManager?.unregisterNetworkCallback(networkCallback)
    }

    @VisibleForTesting
    @WorkerThread
    fun getIntentActivities(shareIntent: Intent, context: Context): List<ResolveInfo>? {
        return context.packageManager.queryIntentActivitiesCompat(shareIntent, 0)
    }

    /**
     * Returns a list of apps that can be shared to.
     *
     * @param intentActivities List of activities from [getIntentActivities].
     * @param context Android context.
     */
    @VisibleForTesting
    @WorkerThread
    internal fun buildAppsList(
        intentActivities: List<ResolveInfo>?,
        context: Context,
    ): List<AppShareOption> {
        return intentActivities
            .orEmpty()
            .filter { it.activityInfo.packageName != context.packageName }
            .map { resolveInfo ->
                AppShareOption(
                    resolveInfo.loadLabel(context.packageManager).toString(),
                    resolveInfo.loadIcon(context.packageManager),
                    resolveInfo.activityInfo.packageName,
                    resolveInfo.activityInfo.name,
                )
            }
    }

    /**
     * Builds list of options to display in the top row of the share sheet.
     * This will primarily include devices that tabs can be sent to, but also options
     * for reconnecting the account or sending to all devices.
     */
    @VisibleForTesting
    @WorkerThread
    internal fun buildDeviceList(accountManager: FxaAccountManager, network: Network? = null): List<SyncShareOption> {
        val account = accountManager.authenticatedAccount()

        return when {
            // No network
            connectivityManager?.isOnline(network) != true -> listOf(SyncShareOption.Offline)
            // No account signed in
            account == null -> listOf(SyncShareOption.SignIn)
            // Account needs to be re-authenticated
            accountManager.accountNeedsReauth() -> listOf(SyncShareOption.Reconnect)
            // Signed in
            else -> {
                val shareableDevices = account.deviceConstellation().state()
                    ?.otherDevices
                    .orEmpty()
                    .filter { it.capabilities.contains(DeviceCapability.SEND_TAB) }
                    .sortedByDescending { it.lastAccessTime }

                val list = mutableListOf<SyncShareOption>()
                if (shareableDevices.isEmpty()) {
                    // Show add device button if there are no devices
                    list.add(SyncShareOption.AddNewDevice)
                }

                shareableDevices.mapTo(list) { SyncShareOption.SingleDevice(it) }

                if (shareableDevices.size > 1) {
                    // Show send all button if there are multiple devices
                    list.add(SyncShareOption.SendAll(shareableDevices))
                }
                list
            }
        }
    }
}
