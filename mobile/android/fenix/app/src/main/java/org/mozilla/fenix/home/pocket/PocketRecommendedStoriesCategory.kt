/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory

/**
 * Category name of the default category from which stories are to be shown
 * if user hasn't explicitly selected others.
 */
const val POCKET_STORIES_DEFAULT_CATEGORY_NAME = "general"

/**
 * In memory cache of Pocket assigned topic of interest for recommended stories.
 * Avoids multiple stories mappings for each time we are interested in their categories.
 *
 * One to many relationship with [PocketRecommendedStory]es.
 *
 * @property name The exact name of each category. Case sensitive.
 * @property stories All [PocketRecommendedStory]s with this category.
 */
data class PocketRecommendedStoriesCategory(
    val name: String,
    val stories: List<PocketRecommendedStory> = emptyList(),
)
