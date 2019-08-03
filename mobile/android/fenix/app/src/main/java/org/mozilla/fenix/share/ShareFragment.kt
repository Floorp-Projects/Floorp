/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Intent
import android.content.Intent.ACTION_SEND
import android.content.Intent.EXTRA_TEXT
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.os.Bundle
import android.os.Parcelable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatDialogFragment
import kotlinx.android.parcel.Parcelize
import kotlinx.android.synthetic.main.fragment_share.view.*
import mozilla.components.concept.sync.DeviceEventOutgoing
import mozilla.components.concept.sync.OAuthAccount
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable

class ShareFragment : AppCompatDialogFragment() {
    interface TabsSharedCallback {
        fun onTabsShared(tabsSize: Int)
    }

    private lateinit var component: ShareComponent
    private var tabs: Array<ShareTab> = emptyArray()

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
        val args = ShareFragmentArgs.fromBundle(arguments!!)
        if (args.url == null && args.tabs.isNullOrEmpty()) {
            throw IllegalStateException("URL and tabs cannot both be null.")
        }

        tabs = args.tabs ?: arrayOf(ShareTab(args.url!!, args.title ?: ""))

        component = ShareComponent(
            view.shareWrapper,
            ActionBusFactory.get(this),
            FenixViewModelProvider.create(
                this,
                ShareUIViewModel::class.java
            ) {
                ShareUIViewModel(ShareState)
            }
        )

        return view
    }

    override fun onResume() {
        super.onResume()
        subscribeToActions()
    }

    @SuppressWarnings("ComplexMethod")
    private fun subscribeToActions() {
        getAutoDisposeObservable<ShareAction>().subscribe {
            when (it) {
                ShareAction.Close -> {
                    dismiss()
                }
                ShareAction.SignInClicked -> {
                    val directions =
                        ShareFragmentDirections.actionShareFragmentToTurnOnSyncFragment()
                    nav(R.id.shareFragment, directions)
                    dismiss()
                }
                ShareAction.AddNewDeviceClicked -> {
                    context?.let {
                        AlertDialog.Builder(it).apply {
                            setMessage(R.string.sync_connect_device_dialog)
                            setPositiveButton(R.string.sync_confirmation_button) { dialog, _ -> dialog.cancel() }
                            create()
                        }.show()
                    }
                }
                is ShareAction.ShareDeviceClicked -> {
                    val authAccount =
                        requireComponents.backgroundServices.accountManager.authenticatedAccount()
                    authAccount?.run {
                        sendSendTab(this, it.device.id, tabs)
                    }
                    dismiss()
                }
                is ShareAction.SendAllClicked -> {
                    val authAccount =
                        requireComponents.backgroundServices.accountManager.authenticatedAccount()
                    authAccount?.run {
                        it.devices.forEach { device ->
                            sendSendTab(this, device.id, tabs)
                        }
                    }
                    dismiss()
                }
                is ShareAction.ShareAppClicked -> {

                    val shareText = tabs.joinToString("\n") { tab -> tab.url }

                    val intent = Intent(ACTION_SEND).apply {
                        putExtra(EXTRA_TEXT, shareText)
                        type = "text/plain"
                        flags = FLAG_ACTIVITY_NEW_TASK
                        setClassName(it.item.packageName, it.item.activityName)
                    }
                    startActivity(intent)
                    dismiss()
                }
            }
        }
    }

    private fun sendSendTab(account: OAuthAccount, deviceId: String, tabs: Array<ShareTab>) {
        account.run {
            tabs.forEach { tab ->
                deviceConstellation().sendEventToDeviceAsync(
                    deviceId,
                    DeviceEventOutgoing.SendTab(tab.title, tab.url)
                )
            }
        }
        (activity as? HomeActivity)?.onTabsShared(tabs.size)
    }
}

@Parcelize
data class ShareTab(val url: String, val title: String, val sessionId: String? = null) : Parcelable
