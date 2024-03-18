/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.robolectric

import android.content.Context
import androidx.test.core.app.ApplicationProvider

/**
 * Provides application context for test purposes
 */
inline val testContext: Context
    get() = ApplicationProvider.getApplicationContext()
