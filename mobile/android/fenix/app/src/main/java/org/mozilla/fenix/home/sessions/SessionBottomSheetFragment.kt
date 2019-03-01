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
import kotlinx.android.synthetic.main.session_bottom_sheet.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import java.lang.IllegalStateException

class SessionBottomSheetFragment : BottomSheetDialogFragment(), LayoutContainer {
    var archivedSession: ArchivedSession? = null
    var isCurrentSession: Boolean = false
    private lateinit var tabTitles: String
    var onDelete: ((ArchivedSession) -> Boolean)? = null
    var onArchive: (() -> Unit)? = null

    override val containerView: View?
        get() = view

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(BottomSheetDialogFragment.STYLE_NORMAL, R.style.CurrentSessionBottomSheetDialogTheme)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.session_bottom_sheet, container, false)
        val snapshot = archivedSession?.bundle?.restoreSnapshot(requireComponents.core.engine)
            ?: throw IllegalStateException()

        tabTitles = snapshot.sessions
            .map { it.session.title }
            .joinToString(", ") {
                if (it.length > maxTitleLength) it.substring(0, maxTitleLength) + "..." else it
            }

        if (!isCurrentSession) {
            view.current_session_card_title.text = archivedSession?.formattedSavedAt
        }

        view.current_session_card_tab_list.text = tabTitles

        view.archive_session_button.apply {
            visibility = if (isCurrentSession) View.VISIBLE else View.GONE
            setOnClickListener {
                onArchive?.invoke()
            }
        }

        view.delete_session_button.setOnClickListener {
            if (onDelete?.invoke(archivedSession!!) == true) {
                dismiss()
            }
        }

        return view
    }

    companion object {
        const val maxTitleLength = 20
        const val overflowFragmentTag = "sessionOverflow"
    }
}
