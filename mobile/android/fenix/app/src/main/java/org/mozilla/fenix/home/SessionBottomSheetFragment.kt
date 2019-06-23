/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.graphics.PorterDuff.Mode.SRC_IN
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.session_bottom_sheet.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.ext.getColorFromAttr
import org.mozilla.fenix.utils.ItsNotBrokenSnack

class SessionBottomSheetFragment : BottomSheetDialogFragment(), LayoutContainer {
    sealed class SessionType {
        data class Current(override val titles: List<String>) : SessionType()
        data class Private(override val titles: List<String>) : SessionType()

        abstract val titles: List<String>
    }

    private var sessionType: SessionType? = null
    var onDelete: ((SessionType) -> Unit)? = null

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
            val drawable = ContextCompat.getDrawable(context!!, R.drawable.ic_tab_collection)
            drawable?.setColorFilter(
                ContextCompat.getColor(
                    context!!,
                    ThemeManager.resolveAttribute(R.attr.accent, context!!)
                ), SRC_IN
            )
            setCompoundDrawablesWithIntrinsicBounds(drawable, null, null, null)
            setOnClickListener {
                dismiss()
            }
        }

        view.delete_session_button.apply {
            val drawable = ContextCompat.getDrawable(context!!, R.drawable.ic_delete)
            drawable?.setColorFilter(
                R.attr.destructive.getColorFromAttr(context), SRC_IN
            )
            setCompoundDrawablesWithIntrinsicBounds(drawable, null, null, null)
        }

        view.send_and_share_session_button.apply {
            val drawable = ContextCompat.getDrawable(context!!, R.drawable.mozac_ic_share)
            drawable?.setColorFilter(
                ContextCompat.getColor(
                    context!!,
                    ThemeManager.resolveAttribute(R.attr.primaryText, context!!)
                ), SRC_IN
            )
            setCompoundDrawablesWithIntrinsicBounds(drawable, null, null, null)
        }

        view.send_and_share_session_button.setOnClickListener {
            ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "244")
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
                is SessionType.Current -> getString(R.string.tab_header_label)
                is SessionType.Private -> getString(R.string.tabs_header_private_title)
            }
        }
    }

    private fun getTabTitles(): String? {
        return sessionType?.titles?.joinToString(", ") {
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
