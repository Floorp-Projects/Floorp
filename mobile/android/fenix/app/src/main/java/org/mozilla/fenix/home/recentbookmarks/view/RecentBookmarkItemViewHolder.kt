/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.View
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.RecentBookmarkItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor
import org.mozilla.fenix.utils.view.ViewHolder

class RecentBookmarkItemViewHolder(
    private val view: View,
    private val interactor: RecentBookmarksInteractor
) : ViewHolder(view) {

    fun bind(bookmark: BookmarkNode) {
        val binding = RecentBookmarkItemBinding.bind(view)

        binding.bookmarkTitle.text = bookmark.title ?: bookmark.url
        binding.bookmarkSubtitle.text = bookmark.url?.tryGetHostFromUrl() ?: bookmark.title ?: ""

        binding.bookmarkItem.setOnClickListener {
            interactor.onRecentBookmarkClicked(bookmark)
        }

        bookmark.url?.let {
            view.context.components.core.icons.loadIntoView(binding.faviconImage, it)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.recent_bookmark_item
    }
}
