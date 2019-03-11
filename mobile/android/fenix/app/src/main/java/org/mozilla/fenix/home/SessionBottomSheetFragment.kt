/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.session_bottom_sheet.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.sessions.ArchivedSession

class SessionBottomSheetFragment : BottomSheetDialogFragment(), LayoutContainer {
    sealed class SessionType {
        data class Current(val titles: List<String>) : SessionType()
        data class Archived(val archivedSession: ArchivedSession) : SessionType()
        data class Private(val titles: List<String>) : SessionType()
    }

    private var sessionType: SessionType? = null
    var onDelete: ((SessionType) -> Unit)? = null
    var onArchive: ((SessionType.Current) -> Unit)? = null

    override val containerView: View?
        get() = view

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(BottomSheetDialogFragment.STYLE_NORMAL, R.style.CurrentSessionBottomSheetDialogTheme)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.session_bottom_sheet, container, false)

        view.current_session_card_title.text = getCardTitle()
        view.current_session_card_tab_list.text = getTabTitles()
        view.archive_session_button.apply {
            visibility = if (sessionType is SessionType.Current) View.VISIBLE else View.GONE
            setOnClickListener {
                sessionType?.also {
                    if (it is SessionType.Current) {
                        onArchive?.invoke(it)
                    }
                }

                dismiss()
            }
        }

        view.delete_session_button.setOnClickListener {
            sessionType?.apply { onDelete?.invoke(this) }
            dismiss()
        }

        return view
    }

    private fun getCardTitle(): String? {
        return sessionType?.let {
            when (it) {
                is SessionType.Archived -> it.archivedSession.formattedSavedAt
                is SessionType.Current -> getString(R.string.tabs_header_title)
                is SessionType.Private -> getString(R.string.tabs_header_private_title)
            }
        }
    }

    private fun getTabTitles(): String? {
        return sessionType?.let {
            when (it) {
                is SessionType.Current -> it.titles
                is SessionType.Private -> it.titles
                is SessionType.Archived ->
                    it.archivedSession.bundle.restoreSnapshot(requireComponents.core.engine)?.let { snapshot ->
                        snapshot.sessions.map { item -> item.session.title }
                    }
            }
        }?.joinToString(", ") {
            if (it.length > maxTitleLength) it.substring(0,
                maxTitleLength
            ) + "..." else it
        }
    }

    companion object {
        const val maxTitleLength = 20
        const val overflowFragmentTag = "sessionOverflow"

        fun create(sessionType: SessionType): SessionBottomSheetFragment {
            val fragment = SessionBottomSheetFragment()
            fragment.sessionType = sessionType
            return fragment
        }
    }
}
