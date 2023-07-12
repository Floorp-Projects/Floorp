/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.state

import mozilla.components.lib.state.Store

/**
 * Store for review quality check feature.
 */
class ReviewQualityCheckStore : Store<ReviewQualityCheckState, ReviewQualityCheckAction>(
    initialState = ReviewQualityCheckState.Initial,
    reducer = { _, _ -> ReviewQualityCheckState.Initial },
)
