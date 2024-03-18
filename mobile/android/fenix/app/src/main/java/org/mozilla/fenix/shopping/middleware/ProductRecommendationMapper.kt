/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.concept.engine.shopping.ProductRecommendation
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.RecommendedProductState
import java.text.NumberFormat
import java.util.Currency
import java.util.Locale

private const val MINIMUM_FRACTION_DIGITS = 0
private const val MAXIMUM_FRACTION_DIGITS = 2

/**
 * Maps [ProductRecommendation] to [RecommendedProductState].
 */
fun ProductRecommendation?.toRecommendedProductState(): RecommendedProductState =
    this?.toRecommendedProduct() ?: RecommendedProductState.Initial

private fun ProductRecommendation.toRecommendedProduct(): RecommendedProductState.Product =
    RecommendedProductState.Product(
        aid = aid,
        name = name,
        productUrl = url,
        imageUrl = imageUrl,
        formattedPrice = price.toDouble().toFormattedAmount(currency),
        reviewGrade = grade.asEnumOrDefault<ReviewQualityCheckState.Grade>()!!,
        adjustedRating = adjustedRating.toFloat(),
        isSponsored = sponsored,
        analysisUrl = analysisUrl,
    )

private fun Double.toFormattedAmount(currencyCode: String): String =
    mapCurrencyCodeToNumberFormat(currencyCode).apply {
        minimumFractionDigits = MINIMUM_FRACTION_DIGITS
        maximumFractionDigits = MAXIMUM_FRACTION_DIGITS
    }.format(this)

private fun mapCurrencyCodeToNumberFormat(currencyCode: String): NumberFormat =
    try {
        val currency = Currency.getInstance(currencyCode)
        NumberFormat.getCurrencyInstance(Locale.getDefault()).apply {
            this.currency = currency
        }
    } catch (e: IllegalArgumentException) {
        NumberFormat.getNumberInstance()
    }
