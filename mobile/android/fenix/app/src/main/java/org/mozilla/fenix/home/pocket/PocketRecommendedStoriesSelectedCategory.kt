/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

/**
 * Details about a selected Pocket recommended stories category.
 *
 * @property name The exact name of a selected category. Case sensitive.
 * @property selectionTimestamp The exact time at which a category was selected. Defaults to [System.currentTimeMillis].
 */
data class PocketRecommendedStoriesSelectedCategory(
    val name: String,
    val selectionTimestamp: Long = System.currentTimeMillis(),
)
