/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.data

import mozilla.components.service.pocket.data.PocketListenArticleMetadata.Status
import mozilla.components.service.pocket.helpers.assertConstructorsVisibility
import org.junit.Assert.assertEquals
import org.junit.Test
import kotlin.reflect.KVisibility

class PocketListenArticleMetadataTest {

    @Test // See constructor comment for details.
    fun `GIVEN a PocketGlobalVideoRecommendation THEN its constructors are internal`() {
        assertConstructorsVisibility(PocketListenArticleMetadata::class, KVisibility.INTERNAL)
    }

    @Test
    fun `WHEN calling Status fromString on known status input THEN the corresponding enum is returned`() {
        listOf(
            "available",
            "avaiLable",
            "AVAILABLE"
        ).forEach { assertEquals(it, Status.AVAILABLE, Status.fromString(it)) }

        assertEquals(Status.PROCESSING, Status.fromString("processing"))
    }

    @Test
    fun `WHEN calling Status fromString on unknown status input THEN the unknown enum is returned`() {
        listOf(
            "",
            "aoseuthaosenuth",
            " available",
            "available "
        ).forEach { assertEquals(it, Status.UNKNOWN, Status.fromString(it)) }
    }

    @Test
    fun `WHEN calling Status fromString THEN the originalString is set`() {
        val expected = "aoesunth"
        val status = Status.fromString(expected)
        assertEquals(expected, status.originalString)
    }
}
