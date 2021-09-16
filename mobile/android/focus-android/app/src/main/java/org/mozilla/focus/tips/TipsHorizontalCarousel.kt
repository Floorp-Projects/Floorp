/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.tips

import android.content.Context
import android.util.AttributeSet
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.PagerSnapHelper
import androidx.recyclerview.widget.RecyclerView

class TipsHorizontalCarousel @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : RecyclerView(context, attrs) {

    val tipsAdapter: TipsAdapter = TipsAdapter()
    private val pagerSnapHelper = PagerSnapHelper()
    private val itemDecoration = DotPagerIndicatorDecoration()

    init {
        layoutManager = LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false)
        adapter = tipsAdapter
        pagerSnapHelper.attachToRecyclerView(this)
        addItemDecoration(itemDecoration)
    }
}
