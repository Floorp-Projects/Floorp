/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.sync

import mozilla.components.service.fxa.SyncEngine
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.rules.ExpectedException

class TypesTest {

    @Test
    fun `raw strings are correctly mapped to SyncEngine types`() {
        assertEquals(SyncEngine.Tabs, "tabs".toSyncEngine())
        assertEquals(SyncEngine.History, "history".toSyncEngine())
        assertEquals(SyncEngine.Bookmarks, "bookmarks".toSyncEngine())
        assertEquals(SyncEngine.Passwords, "passwords".toSyncEngine())
        assertEquals(SyncEngine.CreditCards, "creditcards".toSyncEngine())
        assertEquals(SyncEngine.Addresses, "addresses".toSyncEngine())
        assertEquals(SyncEngine.Other("other"), "other".toSyncEngine())
    }

    @Test
    fun `a list of raw strings are correctly mapped to a set of SyncEngine engines`() {
        assertEquals(
            setOf(SyncEngine.History),
            listOf("history").toSyncEngines(),
        )

        assertEquals(
            setOf(SyncEngine.Bookmarks, SyncEngine.History),
            listOf("history", "bookmarks").toSyncEngines(),
        )

        assertEquals(
            setOf(SyncEngine.History, SyncEngine.CreditCards),
            listOf("history", "creditcards").toSyncEngines(),
        )

        assertEquals(
            setOf(SyncEngine.Other("other"), SyncEngine.CreditCards),
            listOf("other", "creditcards").toSyncEngines(),
        )

        assertEquals(
            setOf(SyncEngine.Bookmarks, SyncEngine.History),
            listOf("history", "bookmarks", "bookmarks", "history").toSyncEngines(),
        )
    }

    @Test
    fun `raw strings are correctly mapped to SyncReason types`() {
        assertEquals(SyncReason.Startup, "startup".toSyncReason())
        assertEquals(SyncReason.FirstSync, "first_sync".toSyncReason())
        assertEquals(SyncReason.Scheduled, "scheduled".toSyncReason())
        assertEquals(SyncReason.User, "user".toSyncReason())
        assertEquals(SyncReason.EngineChange, "engine_change".toSyncReason())
    }

    @Test
    fun `SyncReason types are correctly mapped to strings`() {
        assertEquals("startup", SyncReason.Startup.asString())
        assertEquals("first_sync", SyncReason.FirstSync.asString())
        assertEquals("scheduled", SyncReason.Scheduled.asString())
        assertEquals("user", SyncReason.User.asString())
        assertEquals("engine_change", SyncReason.EngineChange.asString())
    }

    @Rule @JvmField
    val exceptionRule: ExpectedException = ExpectedException.none()

    @Test
    fun `invalid sync reason raw strings throw IllegalStateException when mapped`() {
        exceptionRule.expect(IllegalStateException::class.java)
        exceptionRule.expectMessage("Invalid SyncReason: some_reason")
        "some_reason".toSyncReason()
    }
}
