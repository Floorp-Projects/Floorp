/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import com.leanplum.Leanplum
import com.leanplum.LeanplumActivityHelper
import com.leanplum.annotations.Parser
import org.mozilla.fenix.utils.Settings

object LeanplumHelper {
    fun setupLeanplumIfNeeded(application: FenixApplication) {
        if (!Settings.getInstance(application).isTelemetryEnabled) { return }

        Leanplum.setApplicationContext(application)
        Parser.parseVariables(application)
        LeanplumActivityHelper.enableLifecycleCallbacks(application)
        Leanplum.setAppIdForProductionMode(BuildConfig.LEANPLUM_ID, BuildConfig.LEANPLUM_TOKEN)
        Leanplum.start(application)
    }
}
