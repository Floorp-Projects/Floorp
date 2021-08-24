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
    private val horizontalLayoutManager = HorizontalLayoutManager(context)

    init {
        layoutManager = horizontalLayoutManager
        adapter = tipsAdapter
        pagerSnapHelper.attachToRecyclerView(this)
        addItemDecoration(itemDecoration)
    }

    fun showAsCarousel(state: Boolean) {
        if (state) {
            addItemDecoration(itemDecoration)
            horizontalLayoutManager.setHorizontalScrollEnabled(state)
        } else {
            removeItemDecoration(itemDecoration)
            horizontalLayoutManager.setHorizontalScrollEnabled(state)
        }
    }

    class HorizontalLayoutManager(context: Context?) :
        LinearLayoutManager(context, HORIZONTAL, false) {
        private var isScrollEnabled = true

        fun setHorizontalScrollEnabled(flag: Boolean) {
            isScrollEnabled = flag
        }

        override fun canScrollHorizontally(): Boolean {
            return isScrollEnabled && super.canScrollHorizontally()
        }
    }
}
