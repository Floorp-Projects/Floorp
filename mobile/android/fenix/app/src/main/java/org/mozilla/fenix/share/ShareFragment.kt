/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.os.Bundle
import android.os.Parcelable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatDialogFragment
import kotlinx.android.parcel.Parcelize
import kotlinx.android.synthetic.main.fragment_share.view.*
import org.mozilla.fenix.R

class ShareFragment : AppCompatDialogFragment() {
    interface TabsSharedCallback {
        fun onTabsShared(tabsSize: Int)
    }

    private lateinit var shareInteractor: ShareInteractor
    private lateinit var shareCloseView: ShareCloseView
    private lateinit var shareToAccountDevicesView: ShareToAccountDevicesView
    private lateinit var shareToAppsView: ShareToAppsView
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

        shareInteractor = ShareInteractor()

        shareCloseView = ShareCloseView(view.closeSharingLayout, shareInteractor)
        shareToAccountDevicesView = ShareToAccountDevicesView(view.devicesShareLayout, shareInteractor)
        shareToAppsView = ShareToAppsView(view.appsShareLayout, shareInteractor)

        return view
    }
}

@Parcelize
data class ShareTab(val url: String, val title: String, val sessionId: String? = null) : Parcelable
