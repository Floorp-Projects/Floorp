/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.annotation.SuppressLint
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.WindowManager
import android.widget.PopupWindow
import mozilla.components.support.ktx.android.content.res.pxToDp
import mozilla.components.support.ktx.android.view.isRTL

/**
 * A popup menu composed of BrowserMenuItem objects.
 */
class BrowserMenu internal constructor(
    private val adapter: BrowserMenuAdapter
) {
    private var currentPopup: PopupWindow? = null

    @SuppressLint("InflateParams")
    fun show(anchor: View): PopupWindow {
        val view = LayoutInflater.from(anchor.context).inflate(R.layout.mozac_browser_menu, null)

        adapter.menu = this

        val menuList: RecyclerView = view.findViewById(R.id.mozac_browser_menu_recyclerView)
        menuList.layoutManager = LinearLayoutManager(anchor.context, LinearLayoutManager.VERTICAL, false)
        menuList.adapter = adapter

        return PopupWindow(
                view,
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT
        ).apply {
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            isFocusable = true
            elevation = view.resources.pxToDp(MENU_ELEVATION_DP).toFloat()

            setOnDismissListener {
                adapter.menu = null
                currentPopup = null
            }

            val xOffset = if (anchor.isRTL) -anchor.width else 0
            showAsDropDown(anchor, xOffset, -anchor.height)
        }.also {
            currentPopup = it
        }
    }

    fun dismiss() {
        currentPopup?.dismiss()
    }

    companion object {
        private const val MENU_ELEVATION_DP = 8
    }
}
