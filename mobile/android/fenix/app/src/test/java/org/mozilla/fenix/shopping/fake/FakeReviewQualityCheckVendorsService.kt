/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.fake

import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckVendorsService
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.ProductVendor

class FakeReviewQualityCheckVendorsService(
    private val selectedTabUrl: String? = null,
    private val productVendors: List<ProductVendor> = listOf(
        ProductVendor.BEST_BUY,
        ProductVendor.WALMART,
        ProductVendor.AMAZON,
    ),
) : ReviewQualityCheckVendorsService {
    override fun selectedTabUrl(): String? = selectedTabUrl

    override fun productVendors(): List<ProductVendor> = productVendors
}
