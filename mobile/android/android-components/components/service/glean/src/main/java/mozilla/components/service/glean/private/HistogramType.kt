/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.private

/**
 * Enumeration of the different kinds of histograms supported by metrics based on histograms.
 */
enum class HistogramType {
    Linear,
    Exponential
}
