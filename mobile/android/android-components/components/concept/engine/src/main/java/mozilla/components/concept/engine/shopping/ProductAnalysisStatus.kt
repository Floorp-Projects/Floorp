/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.shopping

/**
 * Holds the result of the analysis status of a shopping product.
 *
 * @property status String indicating the current status of the analysis
 * @property progress Number indicating the progress of the analysis
 */
data class ProductAnalysisStatus(
    val status: String,
    val progress: Double,
)
