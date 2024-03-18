/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.experiments.nimbus.internal.NimbusException

class NimbusSetupKtTest {
    @Test
    fun `WHEN error is reportable THEN return true`() {
        val error = NimbusException.IoException("bad error")

        assertTrue(error.isReportableError())
    }

    @Test
    fun `WHEN error is non-reportable THEN return false`() {
        val error = NimbusException.ClientException("oops")

        assertFalse(error.isReportableError())
    }
}
