/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.components.service.glean.Glean
import java.util.Date

/**
 * Enable testing mode, initialize [WorkManagerTestInitHelper] for testing,
 * and initialize Glean. This is used in tests that send Glean pings.
 *
 * @param context the application context to init Glean with
 */
internal fun resetGlean(
    context: Context = ApplicationProvider.getApplicationContext()
) {
    Glean.enableTestingMode()

    // We're using the WorkManager in a bunch of places, and Glean will crash
    // in tests without this line. Let's simply put it here.
    WorkManagerTestInitHelper.initializeTestWorkManager(context)

    Glean.initialize(context)
}

fun Date.asSeconds() = time / BaseGleanSyncPing.MILLIS_PER_SEC
