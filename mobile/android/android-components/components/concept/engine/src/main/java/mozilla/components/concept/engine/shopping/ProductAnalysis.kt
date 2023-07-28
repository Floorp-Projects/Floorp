/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.shopping

/**
 * Holds the result of the analysis of a shopping product.
 */
interface ProductAnalysis {

    /**
     * Product identifier (ASIN/SKU)
     */
    val productId: String?
}
