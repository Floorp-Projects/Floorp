package org.mozilla.fenix.share

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Intent
import android.content.Intent.ACTION_SEND
import android.content.Intent.EXTRA_TEXT
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.navigation.fragment.NavHostFragment.findNavController
import kotlinx.android.synthetic.main.component_share.*
import kotlinx.android.synthetic.main.fragment_share.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.concept.sync.DeviceEventOutgoing
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import kotlin.coroutines.CoroutineContext

class ShareFragment : AppCompatDialogFragment(), CoroutineScope {
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job
    private lateinit var job: Job
    private lateinit var component: ShareComponent
    private lateinit var url: String
    private lateinit var title: String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.CreateCollectionDialogStyle)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_share, container, false)
        val args = ShareFragmentArgs.fromBundle(arguments!!)

        job = Job()
        url = args.url
        title = args.title ?: ""
        component = ShareComponent(
            view.share_wrapper,
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

    override fun onDestroyView() {
        super.onDestroyView()
        job.cancel()
    }

    @SuppressWarnings("ComplexMethod")
    private fun subscribeToActions() {
        getAutoDisposeObservable<ShareAction>().subscribe {
            when (it) {
                ShareAction.Close -> {
                    dismiss()
                }
                ShareAction.SignInClicked -> {
                    val directions = ShareFragmentDirections.actionShareFragmentToTurnOnSyncFragment()
                    findNavController(this@ShareFragment).navigate(directions)
                }
                ShareAction.AddNewDeviceClicked -> {
                    requireComponents.useCases.tabsUseCases.addTab.invoke(ADD_NEW_DEVICES_URL, true)
                }
                ShareAction.HideSendTab -> {
                    send_tab_group.visibility = View.GONE
                }
                is ShareAction.ShareDeviceClicked -> {
                    val authAccount = requireComponents.backgroundServices.accountManager.authenticatedAccount()
                    authAccount?.run {
                        deviceConstellation().sendEventToDeviceAsync(
                            it.device.id,
                            DeviceEventOutgoing.SendTab(title, url)
                        )
                    }
                }
                is ShareAction.SendAllClicked -> {
                    val authAccount = requireComponents.backgroundServices.accountManager.authenticatedAccount()
                    authAccount?.run {
                        it.devices.forEach { device ->
                            deviceConstellation().sendEventToDeviceAsync(
                                device.id,
                                DeviceEventOutgoing.SendTab(title, url)
                            )
                        }
                    }
                }
                is ShareAction.ShareAppClicked -> {
                    val intent = Intent(ACTION_SEND).apply {
                        putExtra(EXTRA_TEXT, url)
                        type = "text/plain"
                        flags = FLAG_ACTIVITY_NEW_TASK
                        `package` = it.packageName
                    }
                    startActivity(intent)
                }
            }
            dismiss()
        }
    }

    companion object {
        // TODO Replace this link with the correct one when provided.
        const val ADD_NEW_DEVICES_URL = "https://accounts.firefox.com/connect_another_device"
    }
}
