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
import androidx.fragment.app.DialogFragment
import kotlinx.android.synthetic.main.fragment_share.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import kotlin.coroutines.CoroutineContext

class ShareFragment : DialogFragment(), CoroutineScope {
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job
    private lateinit var job: Job
    private lateinit var component: ShareComponent
    private lateinit var url: String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.CreateCollectionDialogStyle)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_share, container, false)
        val args = ShareFragmentArgs.fromBundle(arguments!!)

        job = Job()
        url = args.url
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

    private fun subscribeToActions() {
        getAutoDisposeObservable<ShareAction>().subscribe {
            when (it) {
                ShareAction.Close -> {
                    dismiss()
                }
                ShareAction.AddNewDeviceClicked -> {
                    requireComponents.useCases.tabsUseCases.addTab.invoke(ADD_NEW_DEVICES_URL, true)
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
                // TODO support other actions in a follow-up issue
            }
            dismiss()
        }
    }

    companion object {
        const val ADD_NEW_DEVICES_URL = "https://accounts.firefox.com/connect_another_device"
    }
}
