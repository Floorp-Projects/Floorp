/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import mozilla.components.service.pocket.PocketRecommendedStory

/**
 * Category name of the default category from which stories are to be shown
 * if user hasn't explicitly selected others.
 */
const val POCKET_STORIES_DEFAULT_CATEGORY_NAME = "general"

/**
 * Pocket assigned topic of interest for each story.
 *
 * One to many relationship with [PocketRecommendedStory]es.
 *
 * @property name The exact name of each category. Case sensitive.
 * @property stories All [PocketRecommendedStory]es with this category.
 * @property isSelected Whether this category is currently selected by the user.
 * @property lastInteractedWithTimestamp Last time the user selected or deselected this category.
 */
data class PocketRecommendedStoryCategory(
    val name: String,
    val stories: List<PocketRecommendedStory> = emptyList(),
    val isSelected: Boolean = false,
    val lastInteractedWithTimestamp: Long = 0L
)
