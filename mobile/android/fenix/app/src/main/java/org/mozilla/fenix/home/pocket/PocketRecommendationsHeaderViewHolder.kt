/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber")

package org.mozilla.fenix.home.pocket

import android.view.View
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.unit.dp
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * [RecyclerView.ViewHolder] for displaying the Pocket feature header.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 * @property interactor [PocketStoriesInteractor] callback for user interaction.
 */
class PocketRecommendationsHeaderViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: PocketStoriesInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    @Composable
    override fun Content() {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)

        val wallpaperState = components.appStore
            .observeAsComposableState { state -> state.wallpaperState }.value

        var textColor = FirefoxTheme.colors.textPrimary
        var linkTextColor = FirefoxTheme.colors.textAccent

        wallpaperState?.currentWallpaper?.let { currentWallpaper ->
            currentWallpaper.textColor?.let {
                val wallpaperAdaptedTextColor = Color(it)
                textColor = wallpaperAdaptedTextColor
                linkTextColor = wallpaperAdaptedTextColor
            }
        }

        Column {
            Spacer(Modifier.height(24.dp))

            PoweredByPocketHeader(
                onLearnMoreClicked = interactor::onLearnMoreClicked,
                modifier = Modifier.fillMaxWidth(),
                textColor = textColor,
                linkTextColor = linkTextColor,
            )
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}

@Composable
@LightDarkPreview
private fun PocketRecommendationsFooterViewHolderPreview() {
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            PoweredByPocketHeader(
                onLearnMoreClicked = {},
            )
        }
    }
}
