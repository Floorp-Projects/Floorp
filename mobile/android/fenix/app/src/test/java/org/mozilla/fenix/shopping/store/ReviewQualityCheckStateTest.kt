/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import org.junit.Assert.assertEquals
import org.junit.Test

class ReviewQualityCheckStateTest {

    @Test
    fun `WHEN highlights are present THEN highlights to display in compact mode should contain first 2 highlights of the first highlight type`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.QUALITY to listOf(
                "High quality",
                "Excellent craftsmanship",
                "Superior materials",
            ),
            ReviewQualityCheckState.HighlightType.PRICE to listOf(
                "Affordable prices",
                "Great value for money",
                "Discounted offers",
            ),
            ReviewQualityCheckState.HighlightType.SHIPPING to listOf(
                "Fast and reliable shipping",
                "Free shipping options",
                "Express delivery",
            ),
            ReviewQualityCheckState.HighlightType.PACKAGING_AND_APPEARANCE to listOf(
                "Elegant packaging",
                "Attractive appearance",
                "Beautiful design",
            ),
            ReviewQualityCheckState.HighlightType.COMPETITIVENESS to listOf(
                "Competitive pricing",
                "Strong market presence",
                "Unbeatable deals",
            ),
        )

        val expected = mapOf(
            ReviewQualityCheckState.HighlightType.QUALITY to listOf(
                "High quality",
                "Excellent craftsmanship",
            ),
        )

        assertEquals(expected, highlights.forCompactMode())
    }

    @Test
    fun `WHEN only 1 highlight is present THEN highlights to display in compact mode should contain that one`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.PRICE to listOf(
                "Affordable prices",
            ),
        )

        val expected = mapOf(
            ReviewQualityCheckState.HighlightType.PRICE to listOf(
                "Affordable prices",
            ),
        )

        assertEquals(expected, highlights.forCompactMode())
    }
}
