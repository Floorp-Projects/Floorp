/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions.db

import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import mozilla.components.concept.engine.permission.SitePermissions.Status.NO_DECISION
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class StatusConverterTest {

    @Test
    fun `convert from int to status`() {
        val converter = StatusConverter()

        var status = converter.toStatus(BLOCKED.id)
        assertEquals(status, BLOCKED)

        status = converter.toStatus(NO_DECISION.id)
        assertEquals(status, NO_DECISION)

        status = converter.toStatus(ALLOWED.id)
        assertEquals(status, ALLOWED)

        status = converter.toStatus(Int.MAX_VALUE)
        assertNull(status)
    }

    @Test
    fun `convert from status to int`() {
        val converter = StatusConverter()

        var index = converter.toInt(ALLOWED)
        assertEquals(index, ALLOWED.id)

        index = converter.toInt(BLOCKED)
        assertEquals(index, BLOCKED.id)

        index = converter.toInt(NO_DECISION)
        assertEquals(index, NO_DECISION.id)
    }

    @Test
    fun `convert from int to autoplay status`() {
        val converter = StatusConverter()

        var status = converter.toAutoplayStatus(AutoplayStatus.BLOCKED.id)
        assertEquals(status, AutoplayStatus.BLOCKED)

        status = converter.toAutoplayStatus(AutoplayStatus.ALLOWED.id)
        assertEquals(status, AutoplayStatus.ALLOWED)

        status = converter.toAutoplayStatus(Int.MAX_VALUE)
        assertEquals(AutoplayStatus.BLOCKED, status)
    }

    @Test
    fun `convert from autoplay status to int`() {
        val converter = StatusConverter()

        var index = converter.toInt(AutoplayStatus.ALLOWED)
        assertEquals(index, AutoplayStatus.ALLOWED.id)

        index = converter.toInt(AutoplayStatus.BLOCKED)
        assertEquals(index, AutoplayStatus.BLOCKED.id)
    }
}
