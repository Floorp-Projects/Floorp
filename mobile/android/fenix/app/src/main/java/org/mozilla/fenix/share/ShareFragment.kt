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
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.android.parcel.Parcelize
import kotlinx.android.synthetic.main.fragment_share.view.*
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.sendtab.SendTabUseCases
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbarPresenter
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.share.listadapters.AppShareOption
import org.mozilla.fenix.share.listadapters.SyncShareOption

@Suppress("TooManyFunctions")
class ShareFragment : AppCompatDialogFragment() {
    private lateinit var shareInteractor: ShareInteractor
    private lateinit var shareCloseView: ShareCloseView
    private lateinit var shareToAccountDevicesView: ShareToAccountDevicesView
    private lateinit var shareToAppsView: ShareToAppsView
    private lateinit var appsListDeferred: Deferred<List<AppShareOption>>
    private lateinit var devicesListDeferred: Deferred<List<SyncShareOption>>
    private var connectivityManager: ConnectivityManager? = null

    override fun onAttach(context: Context) {
        super.onAttach(context)

        connectivityManager =
            context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
        val networkRequest = NetworkRequest.Builder().build()
        connectivityManager?.registerNetworkCallback(networkRequest, networkCallback)

        // Start preparing the data as soon as we have a valid Context
        appsListDeferred = lifecycleScope.async(Dispatchers.IO) {
            val shareIntent = Intent(ACTION_SEND).apply {
                type = "text/plain"
                flags = FLAG_ACTIVITY_NEW_TASK
            }
            val shareAppsActivities = getIntentActivities(shareIntent, context)
            buildAppsList(shareAppsActivities, context)
        }

        devicesListDeferred = lifecycleScope.async(Dispatchers.IO) {
            val fxaAccountManager = context.components.backgroundServices.accountManager
            buildDeviceList(fxaAccountManager)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.ShareDialogStyle)
    }

    private val networkCallback = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network?) {
            reloadDevices()
        }

        override fun onAvailable(network: Network?) {
            reloadDevices()
        }
    }

    private fun reloadDevices() {
        context?.let {
            val fxaAccountManager = it.components.backgroundServices.accountManager
            lifecycleScope.launch {
                val refreshDevicesAsync =
                    fxaAccountManager.authenticatedAccount()?.deviceConstellation()
                        ?.refreshDevicesAsync()
                refreshDevicesAsync?.await()
                val devicesShareOptions = buildDeviceList(fxaAccountManager)
                shareToAccountDevicesView.setSharetargets(devicesShareOptions)
            }
        }
    }

    override fun onDetach() {
        connectivityManager?.unregisterNetworkCallback(networkCallback)
        super.onDetach()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_share, container, false)
        val args = ShareFragmentArgs.fromBundle(arguments!!)
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

        lifecycleScope.launch {
            val devicesShareOptions = devicesListDeferred.await()
            shareToAccountDevicesView.setSharetargets(devicesShareOptions)
            val appsToShareTo = appsListDeferred.await()
            shareToAppsView.setSharetargets(appsToShareTo)
        }
    }

    private fun getIntentActivities(shareIntent: Intent, context: Context): List<ResolveInfo>? {
        return context.packageManager.queryIntentActivities(shareIntent, 0)
    }

    private fun buildAppsList(
        intentActivities: List<ResolveInfo>?,
        context: Context
    ): List<AppShareOption> {
        return intentActivities?.map { resolveInfo ->
            AppShareOption(
                resolveInfo.loadLabel(context.packageManager).toString(),
                resolveInfo.loadIcon(context.packageManager),
                resolveInfo.activityInfo.packageName,
                resolveInfo.activityInfo.name
            )
        }?.filter { it.packageName != context.packageName }.orEmpty()
    }

    @Suppress("ReturnCount")
    private fun buildDeviceList(accountManager: FxaAccountManager): List<SyncShareOption> {
        val list = mutableListOf<SyncShareOption>()

        val activeNetwork = connectivityManager?.activeNetworkInfo
        if (activeNetwork?.isConnected != true) {
            list.add(SyncShareOption.Offline)
            return list
        }

        if (accountManager.authenticatedAccount() == null) {
            list.add(SyncShareOption.SignIn)
            return list
        }

        if (accountManager.accountNeedsReauth()) {
            list.add(SyncShareOption.Reconnect)
            return list
        }

        accountManager.authenticatedAccount()?.deviceConstellation()?.state()
            ?.otherDevices?.let { devices ->
            val shareableDevices =
                devices.filter { it.capabilities.contains(DeviceCapability.SEND_TAB) }

            if (shareableDevices.isEmpty()) {
                list.add(SyncShareOption.AddNewDevice)
            }

            val shareOptions = shareableDevices.map {
                when (it.deviceType) {
                    DeviceType.MOBILE -> SyncShareOption.Mobile(it.displayName, it)
                    else -> SyncShareOption.Desktop(it.displayName, it)
                }
            }
            list.addAll(shareOptions)

            if (shareableDevices.size > 1) {
                list.add(SyncShareOption.SendAll(shareableDevices))
            }
        }
        return list
    }

    companion object {
        const val SHOW_PAGE_ALPHA = 0.6f
    }
}

@Parcelize
data class ShareTab(val url: String, val title: String) : Parcelable
