/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.view.doOnNextLayout
import androidx.core.view.updateLayoutParams
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.concept.base.images.ImageLoadRequest
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.TabPreviewBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager
import kotlin.math.max

class TabPreview @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : FrameLayout(context, attrs, defStyle) {

    private val binding = TabPreviewBinding.inflate(LayoutInflater.from(context), this)
    private val thumbnailLoader = ThumbnailLoader(context.components.core.thumbnailStorage)

    init {
        if (!context.settings().shouldUseBottomToolbar) {
            binding.fakeToolbar.updateLayoutParams<LayoutParams> {
                gravity = Gravity.TOP
            }

            binding.fakeToolbar.background = AppCompatResources.getDrawable(
                context,
                ThemeManager.resolveAttribute(R.attr.bottomBarBackgroundTop, context)
            )
        }

        // Change view properties to avoid confusing the UI tests
        binding.tabButton.findViewById<View>(R.id.counter_box).id = View.NO_ID
        binding.tabButton.findViewById<View>(R.id.counter_text).id = View.NO_ID
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)
        binding.previewThumbnail.translationY = if (!context.settings().shouldUseBottomToolbar) {
            binding.fakeToolbar.height.toFloat()
        } else {
            0f
        }
    }

    fun loadPreviewThumbnail(thumbnailId: String) {
        doOnNextLayout {
            val previewThumbnail = binding.previewThumbnail
            val thumbnailSize = max(previewThumbnail.height, previewThumbnail.width)
            thumbnailLoader.loadIntoView(
                previewThumbnail,
                ImageLoadRequest(thumbnailId, thumbnailSize)
            )
        }
    }
}
