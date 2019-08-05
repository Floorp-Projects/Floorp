/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.testing

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import org.junit.rules.TestWatcher
import org.junit.runner.Description

/**
 * This implements a JUnit rule for writing tests for Glean SDK metrics.
 *
 * The rule takes care of resetting the Glean SDK between tests and
 * initializing all the required dependencies.
 *
 * Example usage:
 *
 * ```
 * // Add the following lines to you test class.
 * @get:Rule
 * val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())
 * ```
 */
@VisibleForTesting(otherwise = VisibleForTesting.NONE)
class GleanTestRule(
    val context: Context
) : TestWatcher() {
    override fun starting(description: Description?) {
        // We're using the WorkManager in a bunch of places, and Glean will crash
        // in tests without this line. Let's simply put it here.
        WorkManagerTestInitHelper.initializeTestWorkManager(context)

        Glean.resetGlean(
            context = context,
            config = Configuration(),
            clearStores = true
        )
    }
}
