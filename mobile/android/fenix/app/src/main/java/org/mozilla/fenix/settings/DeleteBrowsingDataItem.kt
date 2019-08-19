/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.withStyledAttributes
import kotlinx.android.synthetic.main.delete_browsing_data_item.view.*
import org.mozilla.fenix.R

class DeleteBrowsingDataItem @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {

    private companion object {
        private const val ENABLED_ALPHA = 1f
        private const val DISABLED_ALPHA = 0.6f
    }

    val titleView: TextView
        get() = title

    val subtitleView: TextView
        get() = subtitle

    var isChecked: Boolean
        get() = checkbox.isChecked
        set(value) { checkbox.isChecked = value }

    var onCheckListener: ((Boolean) -> Unit)? = null

    init {
        LayoutInflater.from(context).inflate(R.layout.delete_browsing_data_item, this, true)

        setOnClickListener {
            checkbox.isChecked = !checkbox.isChecked
        }

        checkbox.setOnCheckedChangeListener { _, isChecked ->
            onCheckListener?.invoke(isChecked)
        }

        context.withStyledAttributes(attrs, R.styleable.DeleteBrowsingDataItem, defStyleAttr, 0) {
            val iconId = getResourceId(
                R.styleable.DeleteBrowsingDataItem_deleteBrowsingDataItemIcon,
                R.drawable.library_icon_reading_list_circle_background
            )
            val titleId = getResourceId(
                R.styleable.DeleteBrowsingDataItem_deleteBrowsingDataItemTitle,
                R.string.browser_menu_your_library
            )
            val subtitleId = getResourceId(
                R.styleable.DeleteBrowsingDataItem_deleteBrowsingDataItemSubtitle,
                R.string.browser_menu_your_library
            )

            icon.background = resources.getDrawable(iconId, context.theme)
            title.text = resources.getString(titleId)
            subtitle.text = resources.getString(subtitleId)
        }
    }

    override fun setEnabled(enabled: Boolean) {
        super.setEnabled(enabled)
        alpha = if (enabled) ENABLED_ALPHA else DISABLED_ALPHA
    }
}
