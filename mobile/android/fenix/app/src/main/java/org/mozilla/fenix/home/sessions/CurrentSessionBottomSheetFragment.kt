/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.current_session_bottom_sheet.view.*
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents

class CurrentSessionBottomSheetFragment : BottomSheetDialogFragment(), LayoutContainer {

    override val containerView: View?
        get() = view

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(BottomSheetDialogFragment.STYLE_NORMAL, R.style.CurrentSessionBottomSheetDialogTheme)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.current_session_bottom_sheet, container, false)
        val sessions = requireComponents.core.sessionManager.sessions.filter {
            (activity as HomeActivity).browsingModeManager.isPrivate == it.private
        }

        view.current_session_card_tab_list.text = sessions.joinToString(", ") {
            if (it.title.length > maxTitleLength) it.title.substring(0, maxTitleLength) + "..." else it.title
        }

        view.delete_session_button.setOnClickListener {
            requireComponents.core.sessionManager.removeAll()
            dismiss()
        }
        return view
    }

    companion object {
        const val maxTitleLength = 20
    }
}
