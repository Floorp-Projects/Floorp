/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.messaging.microsurvey

import androidx.annotation.DrawableRes

/**
 * Model containing the required data from a raw [MicrosurveyUiData] object in a UI state.
 */
data class MicrosurveyUiData(
    val type: Type,
    @DrawableRes val imageRes: Int,
    val title: String,
    val description: String,
    val primaryButtonLabel: String,
    val secondaryButtonLabel: String,
) {
    /**
     * Model for different types of Microsurvey ratings.
     *
     * @property fivePointScaleItems Identifier for the likert scale rating.
     */
    enum class Type(
        val fivePointScaleItems: String,
    ) {
        VERY_DISSATISFIED(
            fivePointScaleItems = "very_dissatisfied",
        ),
        DISSATISFIED(
            fivePointScaleItems = "dissatisfied",
        ),
        NEUTRAL(
            fivePointScaleItems = "neutral",
        ),
        SATISFIED(
            fivePointScaleItems = "satisfied",
        ),
        VERY_SATISFIED(
            fivePointScaleItems = "very_satisfied",
        ),
    }
}
