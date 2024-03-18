/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui.ext

import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import org.mozilla.fenix.R
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.ProductVendor

/**
 * Returns the display string corresponding to the particular [ProductVendor].
 */
@Composable
fun ProductVendor.displayName(): String = when (this) {
    ProductVendor.AMAZON -> stringResource(id = R.string.review_quality_check_retailer_name_amazon)
    ProductVendor.BEST_BUY -> stringResource(id = R.string.review_quality_check_retailer_name_bestbuy)
    ProductVendor.WALMART -> stringResource(id = R.string.review_quality_check_retailer_name_walmart)
}
