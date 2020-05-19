/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.menu.browser

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.PopupWindow
import android.widget.TextView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.CustomTabConfig
import org.mozilla.focus.R
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.utils.ViewUtils.isRTL

/**
 * The overflow menu shown in the BrowserFragment containing page actions like "Refresh", "Share" etc.
 */
class BrowserMenu(
    context: Context,
    fragment: BrowserFragment,
    customTabConfig: CustomTabConfig?
) : PopupWindow() {
    private val adapter: BrowserMenuAdapter

    init {
        @SuppressLint("InflateParams") // This View will have it's params ignored anyway:
        val view = LayoutInflater.from(context).inflate(R.layout.menu, null)

        contentView = view
        adapter = BrowserMenuAdapter(context, this, fragment, customTabConfig)

        val menuList: RecyclerView = view.findViewById(R.id.list)
        menuList.layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
        menuList.adapter = adapter

        if (customTabConfig != null) {
            val brandingView = view.findViewById<TextView>(R.id.branding)
            brandingView.text = context.getString(
                R.string.menu_custom_tab_branding,
                context.getString(R.string.app_name)
            )
            brandingView.visibility = View.VISIBLE
        }

        setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

        isFocusable = true
        height = ViewGroup.LayoutParams.WRAP_CONTENT
        width = ViewGroup.LayoutParams.WRAP_CONTENT
        elevation = context.resources.getDimension(R.dimen.menu_elevation)
    }

    fun updateTrackers(trackers: Int) {
        adapter.updateTrackers(trackers)
    }

    fun updateLoading(loading: Boolean) {
        adapter.updateLoading(loading)
    }

    fun show(anchor: View) {
        val xOffset = if (isRTL(anchor)) -anchor.width else 0
        super.showAsDropDown(anchor, xOffset, -(anchor.height + anchor.paddingBottom))
    }
}
