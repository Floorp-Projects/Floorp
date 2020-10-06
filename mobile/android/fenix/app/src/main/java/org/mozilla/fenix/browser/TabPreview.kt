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
import androidx.core.content.res.ResourcesCompat
import androidx.core.view.doOnNextLayout
import androidx.core.view.updateLayoutParams
import kotlinx.android.synthetic.main.mozac_ui_tabcounter_layout.view.*
import kotlinx.android.synthetic.main.tab_preview.view.*
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.concept.base.images.ImageLoadRequest
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager
import kotlin.math.max

class TabPreview @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : FrameLayout(context, attrs, defStyle) {

    private val thumbnailLoader = ThumbnailLoader(context.components.core.thumbnailStorage)

    init {
        val inflater = LayoutInflater.from(context)
        inflater.inflate(R.layout.tab_preview, this, true)

        if (!context.settings().shouldUseBottomToolbar) {
            fakeToolbar.updateLayoutParams<LayoutParams> {
                gravity = Gravity.TOP
            }

            fakeToolbar.background = ResourcesCompat.getDrawable(
                resources,
                ThemeManager.resolveAttribute(R.attr.bottomBarBackgroundTop, context),
                null
            )
        }

        // Change view properties to avoid confusing the UI tests
        tab_button.counter_box.id = View.NO_ID
        tab_button.counter_text.id = View.NO_ID
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)
        previewThumbnail.translationY = if (!context.settings().shouldUseBottomToolbar) {
            fakeToolbar.height.toFloat()
        } else {
            0f
        }
    }

    fun loadPreviewThumbnail(thumbnailId: String) {
        doOnNextLayout {
            val thumbnailSize = max(previewThumbnail.height, previewThumbnail.width)
            thumbnailLoader.loadIntoView(previewThumbnail, ImageLoadRequest(thumbnailId, thumbnailSize))
        }
    }
}
