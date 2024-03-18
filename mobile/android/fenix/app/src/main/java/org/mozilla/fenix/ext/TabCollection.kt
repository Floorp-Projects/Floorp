/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import androidx.annotation.ColorInt
import androidx.core.content.ContextCompat
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R
import org.mozilla.fenix.collections.DefaultCollectionCreationController
import kotlin.math.abs

/**
 * Selects one of the predefined collection icon colors based on the id.
 */
@ColorInt
fun TabCollection.getIconColor(context: Context): Int {
    val iconColors = context.resources.obtainTypedArray(R.array.collection_icon_colors)
    val index = abs(id % iconColors.length()).toInt()
    val color = iconColors.getColor(
        index,
        ContextCompat.getColor(context, R.color.fx_mobile_icon_color_oncolor),
    )
    iconColors.recycle()
    return color
}

/**
 * Returns the new default name recommendation for a collection
 *
 * Algorithm: Go through all collections, make a list of their names and keep only the default ones.
 * Then get the numbers from all these default names, compute the maximum number and add one.
 */
fun List<TabCollection>.getDefaultCollectionNumber(): Int {
    return (
        this
            .map { it.title }
            .filter { it.matches(Regex("Collection\\s\\d+")) }
            .map {
                Integer.valueOf(
                    it.split(" ")[DefaultCollectionCreationController.DEFAULT_COLLECTION_NUMBER_POSITION],
                )
            }
            .maxOrNull() ?: 0
        ) + DefaultCollectionCreationController.DEFAULT_INCREMENT_VALUE
}
