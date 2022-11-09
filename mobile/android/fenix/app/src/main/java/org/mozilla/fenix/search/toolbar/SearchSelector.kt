/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.content.Context
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.RelativeLayout
import org.mozilla.fenix.databinding.SearchSelectorBinding

/**
 * A search selector menu used in the Browser Toolbar in Edit mode.
 */
internal class SearchSelector @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
) : RelativeLayout(context, attrs, defStyle) {

    private val binding = SearchSelectorBinding.inflate(LayoutInflater.from(context), this)
    private var marginTop: Int = 0

    override fun setLayoutParams(params: ViewGroup.LayoutParams?) {
        if (params is MarginLayoutParams) {
            params.topMargin = marginTop
        }
        super.setLayoutParams(params)
    }

    fun setIcon(icon: Drawable?, contentDescription: String?) {
        binding.icon.setImageDrawable(icon)
        binding.icon.contentDescription = contentDescription
    }

    fun setTopMargin(margin: Int) {
        marginTop = margin
    }
}
