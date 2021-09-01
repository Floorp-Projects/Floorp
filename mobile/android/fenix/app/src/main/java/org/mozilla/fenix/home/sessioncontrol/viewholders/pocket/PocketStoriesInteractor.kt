/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

/**
 * Contract for all possible user interactions with the Pocket recommended stories feature.
 */
interface PocketStoriesInteractor {
    /**
     * Callback for when the user clicked a specific category.
     *
     * @param categoryClicked the just clicked [PocketRecommendedStoryCategory].
     */
    fun onCategoryClick(categoryClicked: PocketRecommendedStoryCategory)
}
