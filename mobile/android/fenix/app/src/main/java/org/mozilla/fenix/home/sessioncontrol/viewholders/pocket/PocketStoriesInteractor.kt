/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import mozilla.components.service.pocket.PocketRecommendedStory

/**
 * Contract for all possible user interactions with the Pocket recommended stories feature.
 */
interface PocketStoriesInteractor {
    /**
     * Callback for when the user clicked a specific category.
     *
     * @param categoryClicked the just clicked [PocketRecommendedStoriesCategory].
     */
    fun onCategoryClick(categoryClicked: PocketRecommendedStoriesCategory)

    /**
     * Callback for then new stories are shown to the user.
     *
     * @param storiesShown the new list of [PocketRecommendedStory]es shown to the user.
     */
    fun onStoriesShown(storiesShown: List<PocketRecommendedStory>)

    /**
     * Callback for when the user clicks an external link.
     *
     * @param link URL clicked.
     */
    fun onExternalLinkClicked(link: String)
}
