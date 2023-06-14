/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.wallpapers.WallpaperState

/**
 * View holder for top sites.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 * @param interactor [TopSiteInteractor] which will have delegated to all user top sites
 * interactions.
 */
class TopSitesViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: TopSiteInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    @Composable
    override fun Content() {
        val topSites =
            components.appStore.observeAsState(emptyList()) { state -> state.topSites }.value
        val wallpaperState = components.appStore
            .observeAsState(WallpaperState.default) { state -> state.wallpaperState }.value

        TopSites(
            topSites = topSites,
            topSiteColors = TopSiteColors.colors(wallpaperState = wallpaperState),
            onTopSiteClick = { topSite ->
                interactor.onSelectTopSite(topSite, topSites.indexOf(topSite))
            },
            onTopSiteLongClick = interactor::onTopSiteLongClicked,
            onOpenInPrivateTabClicked = interactor::onOpenInPrivateTabClicked,
            onRenameTopSiteClicked = interactor::onRenameTopSiteClicked,
            onRemoveTopSiteClicked = interactor::onRemoveTopSiteClicked,
            onSettingsClicked = interactor::onSettingsClicked,
            onSponsorPrivacyClicked = interactor::onSponsorPrivacyClicked,
        )
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}
