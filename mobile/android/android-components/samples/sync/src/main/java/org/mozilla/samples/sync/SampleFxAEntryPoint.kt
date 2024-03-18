/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync

import mozilla.components.concept.sync.FxAEntryPoint

/**
 * An implementation of [FxAEntryPoint] for the sample application.
 */
enum class SampleFxAEntryPoint(override val entryName: String) : FxAEntryPoint {
    HomeMenu("home-menu"),
}
