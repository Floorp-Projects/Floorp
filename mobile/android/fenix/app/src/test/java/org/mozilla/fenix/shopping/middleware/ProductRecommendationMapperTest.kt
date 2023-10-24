/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.LocaleTestRule
import org.mozilla.fenix.shopping.ProductRecommendationTestData
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import java.util.Locale

class ProductRecommendationMapperTest {

    @get:Rule
    val localeTestRule = LocaleTestRule(Locale.US)

    @Test
    fun `WHEN ProductRecommendation is null THEN it is mapped to Error`() {
        val actual = null.toRecommendedProductState()
        val expected = ReviewQualityCheckState.RecommendedProductState.Error

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN ProductRecommendation has data THEN it is mapped to product`() {
        val productRecommendation = ProductRecommendationTestData.productRecommendation()
        val actual = productRecommendation.toRecommendedProductState()
        val expected = ProductRecommendationTestData.product()

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN ProductRecommendation has data with invalid currency code THEN it is mapped to product`() {
        val productRecommendation = ProductRecommendationTestData.productRecommendation(
            price = "100",
            currency = "invalid",
        )
        val actual = productRecommendation.toRecommendedProductState()
        val expected = ProductRecommendationTestData.product(
            formattedPrice = "100",
        )

        assertEquals(expected, actual)
    }
}
