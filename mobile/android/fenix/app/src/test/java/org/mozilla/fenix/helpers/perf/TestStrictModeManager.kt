/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers.perf

import android.os.StrictMode
import io.mockk.mockk
import org.mozilla.fenix.perf.StrictModeManager

/**
 * A test version of [StrictModeManager]. This class is difficult to mock because of [resetAfter]
 * so we provide a test implementation.
 */
class TestStrictModeManager : StrictModeManager(mockk(relaxed = true), mockk(relaxed = true)) {

    // This method is hard to mock because this method needs to return the return value of the
    // function passed in.
    override fun <R> resetAfter(policy: StrictMode.ThreadPolicy, functionBlock: () -> R): R {
        return functionBlock()
    }
}
