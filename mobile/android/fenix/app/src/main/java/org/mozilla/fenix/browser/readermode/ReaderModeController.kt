/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.readermode

import android.view.View
import android.widget.Button
import android.widget.RadioButton
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.R

/**
 * An interface that exposes the hide and show reader view functions of a ReaderViewFeature
 */
interface ReaderModeController {
    fun hideReaderView()
    fun showReaderView()
    fun showControls()
}

class DefaultReaderModeController(
    private val readerViewFeature: ViewBoundFeatureWrapper<ReaderViewFeature>,
    private val readerViewControlsBar: View,
    private val isPrivate: Boolean = false,
    private val onReaderModeChanged: () -> Unit = {},
) : ReaderModeController {

    @VisibleForTesting
    internal val privateButtonColor
        get() = AppCompatResources.getColorStateList(
            readerViewControlsBar.context,
            R.color.readerview_private_button_color,
        )

    @VisibleForTesting
    internal val privateRadioButtonColor
        get() = AppCompatResources.getColorStateList(
            readerViewControlsBar.context,
            R.color.readerview_private_radio_color,
        )

    override fun hideReaderView() {
        onReaderModeChanged()
        readerViewFeature.withFeature {
            it.hideReaderView()
            it.hideControls()
        }
    }

    override fun showReaderView() {
        onReaderModeChanged()
        readerViewFeature.withFeature { it.showReaderView() }
    }

    override fun showControls() {
        readerViewFeature.withFeature { it.showControls() }
        if (isPrivate) {
            // We need to update styles for private mode programmatically for now:
            // https://github.com/mozilla-mobile/android-components/issues/3400
            themeReaderViewControlsForPrivateMode(readerViewControlsBar)
        }
    }

    private fun themeReaderViewControlsForPrivateMode(view: View) = with(view) {
        listOf(
            R.id.mozac_feature_readerview_font_size_decrease,
            R.id.mozac_feature_readerview_font_size_increase,
        ).map {
            findViewById<Button>(it)
        }.forEach {
            it.setTextColor(privateButtonColor)
        }

        listOf(
            R.id.mozac_feature_readerview_font_serif,
            R.id.mozac_feature_readerview_font_sans_serif,
        ).map {
            findViewById<RadioButton>(it)
        }.forEach {
            it.setTextColor(privateRadioButtonColor)
        }
    }
}
