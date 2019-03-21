/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import androidx.constraintlayout.widget.ConstraintLayout
import kotlinx.android.synthetic.main.library_list_item.view.*
import org.mozilla.fenix.R

class LibraryListItem @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {
    init {
        LayoutInflater.from(context).inflate(R.layout.library_list_item, this, true)

        attrs.let {
            context.theme.obtainStyledAttributes(
                it,
                R.styleable.LibraryListItem,
                0, 0
            ).apply {
                try {
                    val id = getResourceId(
                        R.styleable.LibraryListItem_listItemIcon,
                        R.drawable.library_icon_reading_list_circle_background
                    )
                    libraryIcon?.background = resources.getDrawable(id, context.theme)
                    libraryItemTitle?.text = resources.getString(
                        getResourceId(
                            R.styleable.LibraryListItem_listItemTitle,
                            R.string.browser_menu_library
                        )
                    )
                } finally {
                    recycle()
                }
            }
        }
    }
}
