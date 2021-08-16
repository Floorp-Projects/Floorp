/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.firstrun

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import androidx.viewpager.widget.PagerAdapter
import androidx.viewpager.widget.ViewPager
import org.mozilla.focus.R

class FirstrunPagerAdapter(
    private val context: Context,
    private val listener: View.OnClickListener
) : PagerAdapter() {

    private data class FirstrunPage(val title: String, val text: String, val imageResource: Int) {
        val contentDescription = title + text
    }

    private val pages: Array<FirstrunPage>

    init {
        val appName = context.getString(R.string.app_name)
        this.pages = arrayOf(
            FirstrunPage(
                context.getString(R.string.firstrun_defaultbrowser_title),
                context.getString(R.string.firstrun_defaultbrowser_text2),
                R.drawable.onboarding_img1
            ),
            FirstrunPage(
                context.getString(R.string.firstrun_search_title),
                context.getString(R.string.firstrun_search_text),
                R.drawable.onboarding_img4
            ),
            FirstrunPage(
                context.getString(R.string.firstrun_shortcut_title),
                context.getString(R.string.firstrun_shortcut_text, appName),
                R.drawable.onboarding_img3
            ),
            FirstrunPage(
                context.getString(R.string.firstrun_privacy_title),
                context.getString(R.string.firstrun_privacy_text, appName),
                R.drawable.onboarding_img2
            )
        )
    }

    private fun getView(position: Int, pager: ViewPager): View {
        val view = LayoutInflater.from(context).inflate(R.layout.firstrun_page, pager, false)

        val page = pages[position]

        val titleView: TextView = view.findViewById(R.id.title)
        titleView.text = page.title

        val textView: TextView = view.findViewById(R.id.text)
        textView.text = page.text

        val imageView: ImageView = view.findViewById(R.id.image)
        imageView.setImageResource(page.imageResource)

        val buttonView: Button = view.findViewById(R.id.button)
        buttonView.setOnClickListener(listener)
        if (position == pages.size - 1) {
            buttonView.setText(R.string.firstrun_close_button)
            buttonView.id = R.id.finish
            buttonView.contentDescription = buttonView.text.toString().lowercase()
        } else {
            buttonView.setText(R.string.firstrun_next_button)
            buttonView.id = R.id.next
        }

        return view
    }

    fun getPageAccessibilityDescription(position: Int): String =
        pages[position].contentDescription

    override fun isViewFromObject(view: View, any: Any) = view === any

    override fun getCount() = pages.size

    override fun instantiateItem(container: ViewGroup, position: Int): Any {
        container as ViewPager

        val view = getView(position, container)
        container.addView(view)

        return view
    }

    override fun destroyItem(container: ViewGroup, position: Int, view: Any) {
        view as View
        container.removeView(view)
    }
}
