/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_SEND
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.content.pm.ResolveInfo
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkRequest
import android.os.Bundle
import android.os.Parcelable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.WorkerThread
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.content.getSystemService
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.parcel.Parcelize
import kotlinx.android.synthetic.main.fragment_share.view.*
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.feature.sendtab.SendTabUseCases
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbarPresenter
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.share.listadapters.AndroidShareOption
import org.mozilla.fenix.share.listadapters.SyncShareOption

@Suppress("TooManyFunctions")
class ShareFragment : AppCompatDialogFragment() {
    private lateinit var shareInteractor: ShareInteractor
    private lateinit var shareCloseView: ShareCloseView
    private lateinit var shareToAccountDevicesView: ShareToAccountDevicesView
    private lateinit var shareToAppsView: ShareToAppsView
    private lateinit var appsListDeferred: Deferred<List<AndroidShareOption>>
    private lateinit var devicesListDeferred: Deferred<List<SyncShareOption>>
    private var connectivityManager: ConnectivityManager? = null

    private val networkCallback = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network?) = reloadDevices()
        override fun onAvailable(network: Network?) = reloadDevices()

        private fun reloadDevices() {
            context?.let { context ->
                val fxaAccountManager = context.components.backgroundServices.accountManager
                lifecycleScope.launch {
                    fxaAccountManager.authenticatedAccount()
                        ?.deviceConstellation()
                        ?.refreshDevicesAsync()
                        ?.await()

                    val devicesShareOptions = buildDeviceList(fxaAccountManager)
                    shareToAccountDevicesView.setShareTargets(devicesShareOptions)
                }
            }
        }
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)

        connectivityManager = context.getSystemService()
        val networkRequest = NetworkRequest.Builder().build()
        connectivityManager?.registerNetworkCallback(networkRequest, networkCallback)

        // Start preparing the data as soon as we have a valid Context
        appsListDeferred = lifecycleScope.async(IO) {
            val shareIntent = Intent(ACTION_SEND).apply {
                type = "text/plain"
                flags = FLAG_ACTIVITY_NEW_TASK
            }
            val shareAppsActivities = getIntentActivities(shareIntent, context)
            buildAppsList(shareAppsActivities, context)
        }

        devicesListDeferred = lifecycleScope.async(IO) {
            val fxaAccountManager = context.components.backgroundServices.accountManager
            buildDeviceList(fxaAccountManager)
        }
    }

    override fun onDetach() {
        connectivityManager?.unregisterNetworkCallback(networkCallback)
        super.onDetach()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.ShareDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_share, container, false)
        val args by navArgs<ShareFragmentArgs>()
        check(!(args.url == null && args.tabs.isNullOrEmpty())) { "URL and tabs cannot both be null." }

        val tabs = args.tabs?.toList() ?: listOf(ShareTab(args.url!!, args.title.orEmpty()))
        val accountManager = requireComponents.backgroundServices.accountManager

        shareInteractor = ShareInteractor(
            DefaultShareController(
                context = requireContext(),
                sharedTabs = tabs,
                snackbarPresenter = FenixSnackbarPresenter(activity!!.getRootView()!!),
                navController = findNavController(),
                sendTabUseCases = SendTabUseCases(accountManager),
                dismiss = ::dismiss
            )
        )

        view.shareWrapper.setOnClickListener { shareInteractor.onShareClosed() }
        shareToAccountDevicesView =
            ShareToAccountDevicesView(view.devicesShareLayout, shareInteractor)

        if (args.url != null && args.tabs == null) {
            // If sharing one tab from the browser fragment, show it.
            // If URL is set and tabs is null, we assume the browser is visible, since navigation
            // does not tell us the back stack state.
            view.closeSharingScrim.alpha = SHOW_PAGE_ALPHA
            view.shareWrapper.setOnClickListener { shareInteractor.onShareClosed() }
        } else {
            // Otherwise, show a list of tabs to share.
            view.closeSharingScrim.alpha = 1.0f
            shareCloseView = ShareCloseView(view.closeSharingContent, shareInteractor)
            shareCloseView.setTabs(tabs)
        }
        shareToAppsView = ShareToAppsView(view.appsShareLayout, shareInteractor)

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Start with some invisible views so the share menu height doesn't jump later
        shareToAppsView.setShareTargets(
            listOf(AndroidShareOption.Invisible, AndroidShareOption.Invisible)
        )

        lifecycleScope.launch {
            val devicesShareOptions = devicesListDeferred.await()
            shareToAccountDevicesView.setShareTargets(devicesShareOptions)
            val appsToShareTo = appsListDeferred.await()
            shareToAppsView.setShareTargets(appsToShareTo)
        }
    }

    @WorkerThread
    private fun getIntentActivities(shareIntent: Intent, context: Context): List<ResolveInfo>? {
        return context.packageManager.queryIntentActivities(shareIntent, 0)
    }

    /**
     * Returns a list of apps that can be shared to.
     * @param intentActivities List of activities from [getIntentActivities].
     */
    @WorkerThread
    private fun buildAppsList(
        intentActivities: List<ResolveInfo>?,
        context: Context
    ): List<AndroidShareOption> {
        return intentActivities
            .orEmpty()
            .filter { it.activityInfo.packageName != context.packageName }
            .map { resolveInfo ->
                AndroidShareOption.App(
                    resolveInfo.loadLabel(context.packageManager).toString(),
                    resolveInfo.loadIcon(context.packageManager),
                    resolveInfo.activityInfo.packageName,
                    resolveInfo.activityInfo.name
                )
            }
    }

    /**
     * Builds list of options to display in the top row of the share sheet.
     * This will primarily include devices that tabs can be sent to, but also options
     * for reconnecting the account or sending to all devices.
     */
    private fun buildDeviceList(accountManager: FxaAccountManager): List<SyncShareOption> {
        val activeNetwork = connectivityManager?.activeNetworkInfo
        val account = accountManager.authenticatedAccount()

        return when {
            // No network
            activeNetwork?.isConnected != true -> listOf(SyncShareOption.Offline)
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

    companion object {
        const val SHOW_PAGE_ALPHA = 0.6f
    }
}

@Parcelize
data class ShareTab(val url: String, val title: String) : Parcelable
