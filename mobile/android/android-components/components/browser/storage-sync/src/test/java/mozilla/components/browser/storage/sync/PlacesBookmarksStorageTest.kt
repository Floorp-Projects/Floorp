/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import mozilla.appservices.places.BookmarkUpdateInfo
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.appservices.sync15.EngineInfo
import mozilla.appservices.sync15.FailureName
import mozilla.appservices.sync15.FailureReason
import mozilla.appservices.sync15.OutgoingInfo
import mozilla.appservices.sync15.ProblemInfo
import mozilla.appservices.sync15.SyncInfo
import mozilla.appservices.sync15.SyncTelemetryPing
import mozilla.appservices.sync15.ValidationInfo
import mozilla.components.browser.storage.sync.GleanMetrics.BookmarksSync
import mozilla.components.browser.storage.sync.GleanMetrics.Pings
import mozilla.components.concept.storage.BookmarkInfo
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.util.Date

@RunWith(AndroidJUnit4::class)
class PlacesBookmarksStorageTest {
    @get:Rule
    val gleanRule = GleanTestRule(testContext)

    private var conn: Connection? = null
    private var reader: PlacesReaderConnection? = null
    private var writer: PlacesWriterConnection? = null

    private var storage: PlacesBookmarksStorage? = null

    private val newItem = BookmarkNode(BookmarkNodeType.ITEM, "123", "456", null,
            "Mozilla", "http://www.mozilla.org", null)
    private val newFolder = BookmarkNode(BookmarkNodeType.FOLDER, "789", "321", null,
            "Cool Sites", null, listOf())
    private val newSeparator = BookmarkNode(BookmarkNodeType.SEPARATOR, "654", "987",
            null, null, null, null)

    internal class TestablePlacesBookmarksStorage(override val places: Connection) : PlacesBookmarksStorage(testContext)

    @Before
    fun setup() {
        conn = mock()
        reader = mock()
        writer = mock()
        `when`(conn!!.reader()).thenReturn(reader)
        `when`(conn!!.writer()).thenReturn(writer)
        storage = TestablePlacesBookmarksStorage(conn!!)
    }

    @Test
    fun `get bookmarks tree by root, recursive or not`() {
        val reader = reader!!
        val storage = storage!!

        runBlocking {
            storage.getTree(BookmarkRoot.Root.id)
        }
        verify(reader, times(1)).getBookmarksTree(BookmarkRoot.Root.id, false)

        runBlocking {
            storage.getTree(BookmarkRoot.Root.id, true)
        }
        verify(reader, times(1)).getBookmarksTree(BookmarkRoot.Root.id, true)
    }

    @Test
    fun `get bookmarks by URL`() {
        val reader = reader!!
        val storage = storage!!

        val url = "http://www.mozilla.org"

        runBlocking {
            storage.getBookmarksWithUrl(url)
        }
        verify(reader, times(1)).getBookmarksWithURL(url)
    }

    @Test
    fun `get bookmark by guid`() {
        val reader = reader!!
        val storage = storage!!

        val guid = "123"

        runBlocking {
            storage.getBookmark(guid)
        }
        verify(reader, times(1)).getBookmark(guid)
    }

    @Test
    fun `search bookmarks by keyword`() {
        val reader = reader!!
        val storage = storage!!

        runBlocking {
            storage.searchBookmarks("mozilla")
        }
        verify(reader, times(1)).searchBookmarks("mozilla", 10)

        runBlocking {
            storage.searchBookmarks("mozilla", 30)
        }
        verify(reader, times(1)).searchBookmarks("mozilla", 30)
    }

    @Test
    fun `add a bookmark item`() {
        val writer = writer!!
        val storage = storage!!

        runBlocking {
            storage.addItem(BookmarkRoot.Mobile.id, newItem.url!!, newItem.title!!, null)
        }
        verify(writer, times(1)).createBookmarkItem(
                BookmarkRoot.Mobile.id, "http://www.mozilla.org", "Mozilla", null)

        runBlocking {
            storage.addItem(BookmarkRoot.Mobile.id, newItem.url!!, newItem.title!!, 3)
        }
        verify(writer, times(1)).createBookmarkItem(
                BookmarkRoot.Mobile.id, "http://www.mozilla.org", "Mozilla", 3)
    }

    @Test
    fun `add a bookmark folder`() {
        val writer = writer!!
        val storage = storage!!

        runBlocking {
            storage.addFolder(BookmarkRoot.Mobile.id, newFolder.title!!, null)
        }
        verify(writer, times(1)).createFolder(
                BookmarkRoot.Mobile.id, "Cool Sites", null)

        runBlocking {
            storage.addFolder(BookmarkRoot.Mobile.id, newFolder.title!!, 4)
        }
        verify(writer, times(1)).createFolder(
                BookmarkRoot.Mobile.id, "Cool Sites", 4)
    }

    @Test
    fun `add a bookmark separator`() {
        val writer = writer!!
        val storage = storage!!

        runBlocking {
            storage.addSeparator(BookmarkRoot.Mobile.id, null)
        }
        verify(writer, times(1)).createSeparator(
                BookmarkRoot.Mobile.id, null)

        runBlocking {
            storage.addSeparator(BookmarkRoot.Mobile.id, 4)
        }
        verify(writer, times(1)).createSeparator(
                BookmarkRoot.Mobile.id, 4)
    }

    @Test
    fun `move a bookmark item`() {
        val writer = writer!!
        val storage = storage!!
        val info = BookmarkInfo(newItem.parentGuid, newItem.position, newItem.title, newItem.url)

        runBlocking {
            storage.updateNode(BookmarkRoot.Mobile.id, info)
        }
        verify(writer, times(1)).updateBookmark(
                BookmarkRoot.Mobile.id, info.asBookmarkUpdateInfo())

        runBlocking {
            storage.updateNode(BookmarkRoot.Mobile.id, info.copy(position = 4))
        }
        verify(writer, times(1)).updateBookmark(
                BookmarkRoot.Mobile.id, info.copy(position = 4).asBookmarkUpdateInfo())
    }

    @Test
    fun `move a bookmark folder and its contents`() {
        val writer = writer!!
        val storage = storage!!
        val info = BookmarkInfo(newFolder.parentGuid, newFolder.position, newFolder.title, newFolder.url)

        runBlocking {
            storage.updateNode(BookmarkRoot.Mobile.id, info)
        }
        verify(writer, times(1)).updateBookmark(
                BookmarkRoot.Mobile.id, info.asBookmarkUpdateInfo()
        )
        runBlocking {
            storage.updateNode(BookmarkRoot.Mobile.id, info.copy(position = 5))
        }
        verify(writer, times(1)).updateBookmark(
                BookmarkRoot.Mobile.id, info.copy(position = 5).asBookmarkUpdateInfo()
        )
    }

    @Test
    fun `move a bookmark separator`() {
        val writer = writer!!
        val storage = storage!!
        val info = BookmarkInfo(newSeparator.parentGuid, newSeparator.position, newSeparator.title, newSeparator.url)

        runBlocking {
            storage.updateNode(BookmarkRoot.Mobile.id, info)
        }
        verify(writer, times(1)).updateBookmark(
                BookmarkRoot.Mobile.id, info.asBookmarkUpdateInfo()
        )
        runBlocking {
            storage.updateNode(BookmarkRoot.Mobile.id, info.copy(position = 6))
        }
        verify(writer, times(1)).updateBookmark(BookmarkRoot.Mobile.id, info.copy(position = 6).asBookmarkUpdateInfo())
    }

    @Test
    fun `update a bookmark item`() {
        val writer = writer!!
        val storage = storage!!

        val info = BookmarkInfo("121", 1, "Firefox", "https://www.mozilla.org/en-US/firefox/")

        runBlocking {
            storage.updateNode(newItem.guid, info)
        }
        verify(writer, times(1)).updateBookmark("123", info.asBookmarkUpdateInfo())
    }

    @Test
    fun `update a bookmark folder`() {
        val writer = writer!!
        val storage = storage!!

        val info = BookmarkInfo("131", 2, "Firefox", null)

        runBlocking {
            storage.updateNode(newFolder.guid, info)
        }
        verify(writer, times(1)).updateBookmark(newFolder.guid, info.asBookmarkUpdateInfo())
    }

    @Test
    fun `delete a bookmark item`() {
        val writer = writer!!
        val storage = storage!!

        runBlocking {
            storage.deleteNode(newItem.guid)
        }
        verify(writer, times(1)).deleteBookmarkNode(newItem.guid)
    }

    @Test
    fun `delete a bookmark separator`() {
        val writer = writer!!
        val storage = storage!!

        runBlocking {
            storage.deleteNode(newSeparator.guid)
        }
        verify(writer, times(1)).deleteBookmarkNode(newSeparator.guid)
    }

    @Test
    fun `sends bookmarks telemetry pings on success`() = runBlocking {
        val now = Date().asSeconds()
        val conn = object : Connection {
            var pingCount = 0

            override fun reader(): PlacesReaderConnection {
                Assert.fail()
                return mock()
            }

            override fun writer(): PlacesWriterConnection {
                Assert.fail()
                return mock()
            }

            override fun syncHistory(syncInfo: SyncAuthInfo) {
                Assert.fail()
            }

            override fun sendHistoryPing() {
                Assert.fail()
            }

            override fun syncBookmarks(syncInfo: SyncAuthInfo) {
                assembleBookmarksPing(SyncTelemetryPing(
                    version = 1,
                    uid = "xyz789",
                    syncs = listOf(
                        SyncInfo(
                            at = now + 20,
                            took = 8000,
                            engines = listOf(
                                EngineInfo(
                                    name = "bookmarks",
                                    at = now + 25,
                                    took = 6000,
                                    incoming = null,
                                    outgoing = listOf(
                                        OutgoingInfo(
                                            sent = 10,
                                            failed = 5
                                        )
                                    ),
                                    failureReason = null,
                                    validation = ValidationInfo(
                                        version = 2,
                                        problems = listOf(
                                            ProblemInfo(
                                                name = "missingParents",
                                                count = 5
                                            ),
                                            ProblemInfo(
                                                name = "missingChildren",
                                                count = 7
                                            )
                                        ),
                                        failureReason = null
                                    )
                                )
                            ),
                            failureReason = null
                        )
                    ),
                    events = emptyList()
                ))
            }

            override fun sendBookmarksPing() {
                when (pingCount) {
                    0 -> {
                        BookmarksSync.apply {
                            Assert.assertEquals("xyz789", uid.testGetValue())
                            Assert.assertEquals(now + 25, startedAt.testGetValue().asSeconds())
                            Assert.assertEquals(now + 31, finishedAt.testGetValue().asSeconds())
                            Assert.assertFalse(incoming["applied"].testHasValue())
                            Assert.assertFalse(incoming["failed_to_apply"].testHasValue())
                            Assert.assertFalse(incoming["reconciled"].testHasValue())
                            Assert.assertEquals(10, outgoing["uploaded"].testGetValue())
                            Assert.assertEquals(5, outgoing["failed_to_upload"].testGetValue())
                            Assert.assertEquals(1, outgoingBatches.testGetValue())
                        }
                    }
                    else -> Assert.fail()
                }
                Pings.bookmarksSync.send()
                pingCount++
            }

            override fun close() {
                Assert.fail()
            }
        }
        val storage = TestablePlacesBookmarksStorage(conn)

        val result = storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))

        Assert.assertTrue(result is SyncStatus.Ok)
        Assert.assertEquals(conn.pingCount, 1)
    }

    @Test
    fun `sends bookmarks telemetry pings on engine failure`() = runBlocking {
        val now = Date().asSeconds()
        val conn = object : Connection {
            var pingCount = 0

            override fun reader(): PlacesReaderConnection {
                Assert.fail()
                return mock()
            }

            override fun writer(): PlacesWriterConnection {
                Assert.fail()
                return mock()
            }

            override fun syncHistory(syncInfo: SyncAuthInfo) {
                Assert.fail()
            }

            override fun sendHistoryPing() {
                Assert.fail()
            }

            override fun syncBookmarks(syncInfo: SyncAuthInfo) {
                assembleBookmarksPing(SyncTelemetryPing(
                    version = 1,
                    uid = "abc123",
                    syncs = listOf(
                        SyncInfo(
                            at = now,
                            took = 5000,
                            engines = listOf(
                                EngineInfo(
                                    name = "history",
                                    at = now + 1,
                                    took = 1000,
                                    incoming = null,
                                    outgoing = emptyList(),
                                    failureReason = FailureReason(FailureName.Unknown, "Boxes not locked"),
                                    validation = null
                                ),
                                EngineInfo(
                                    name = "bookmarks",
                                    at = now + 2,
                                    took = 1000,
                                    incoming = null,
                                    outgoing = emptyList(),
                                    failureReason = FailureReason(FailureName.Shutdown),
                                    validation = null
                                ),
                                EngineInfo(
                                    name = "bookmarks",
                                    at = now + 3,
                                    took = 1000,
                                    incoming = null,
                                    outgoing = emptyList(),
                                    failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned"),
                                    validation = null
                                ),
                                EngineInfo(
                                    name = "bookmarks",
                                    at = now + 4,
                                    took = 1000,
                                    incoming = null,
                                    outgoing = emptyList(),
                                    failureReason = FailureReason(FailureName.Http, code = 418),
                                    validation = null
                                )
                            ),
                            failureReason = null
                        ),
                        SyncInfo(
                            at = now + 5,
                            took = 4000,
                            engines = listOf(
                                EngineInfo(
                                    name = "bookmarks",
                                    at = now + 6,
                                    took = 1000,
                                    incoming = null,
                                    outgoing = emptyList(),
                                    failureReason = FailureReason(FailureName.Auth, "Splines not reticulated", 999),
                                    validation = null
                                ),
                                EngineInfo(
                                    name = "bookmarks",
                                    at = now + 7,
                                    took = 1000,
                                    incoming = null,
                                    outgoing = emptyList(),
                                    failureReason = FailureReason(FailureName.Unexpected, "Kaboom!"),
                                    validation = null
                                ),
                                EngineInfo(
                                    name = "bookmarks",
                                    at = now + 8,
                                    took = 1000,
                                    incoming = null,
                                    outgoing = emptyList(),
                                    failureReason = FailureReason(FailureName.Other, "Qualia unsynchronized"), // other
                                    validation = null
                                )
                            ),
                            failureReason = null
                        )
                    ),
                    events = emptyList()
                ))
            }

            override fun sendBookmarksPing() {
                when (pingCount) {
                    0 -> {
                        // Shutdown errors shouldn't be reported.
                        Assert.assertTrue(listOf(
                            "other",
                            "unexpected",
                            "auth"
                        ).none { BookmarksSync.failureReason[it].testHasValue() })
                    }
                    1 -> BookmarksSync.apply {
                        Assert.assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                        Assert.assertFalse(failureReason["unexpected"].testHasValue())
                        Assert.assertFalse(failureReason["auth"].testHasValue())
                    }
                    2 -> BookmarksSync.apply {
                        Assert.assertEquals("Unexpected error: 418", failureReason["unexpected"].testGetValue())
                        Assert.assertFalse(failureReason["other"].testHasValue())
                        Assert.assertFalse(failureReason["auth"].testHasValue())
                    }
                    3 -> BookmarksSync.apply {
                        Assert.assertEquals("Splines not reticulated", failureReason["auth"].testGetValue())
                        Assert.assertFalse(failureReason["other"].testHasValue())
                        Assert.assertFalse(failureReason["unexpected"].testHasValue())
                    }
                    4 -> BookmarksSync.apply {
                        Assert.assertEquals("Kaboom!", failureReason["unexpected"].testGetValue())
                        Assert.assertFalse(failureReason["other"].testHasValue())
                        Assert.assertFalse(failureReason["auth"].testHasValue())
                    }
                    5 -> BookmarksSync.apply {
                        Assert.assertEquals("Qualia unsynchronized", failureReason["other"].testGetValue())
                        Assert.assertFalse(failureReason["unexpected"].testHasValue())
                        Assert.assertFalse(failureReason["auth"].testHasValue())
                    }
                    else -> Assert.fail()
                }
                // We still need to send the ping, so that the counters are
                // cleared out between calls to `sendBookmarksPing`.
                Pings.bookmarksSync.send()
                pingCount++
            }

            override fun close() {
                Assert.fail()
            }
        }
        val storage = TestablePlacesBookmarksStorage(conn)

        val result = storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))

        Assert.assertTrue(result is SyncStatus.Ok)
        Assert.assertEquals(6, conn.pingCount)
    }

    @Test
    fun `sends bookmarks telemetry pings on sync failure`() = runBlocking {
        val now = Date().asSeconds()
        val conn = object : Connection {
            var pingCount = 0

            override fun reader(): PlacesReaderConnection {
                Assert.fail()
                return mock()
            }

            override fun writer(): PlacesWriterConnection {
                Assert.fail()
                return mock()
            }

            override fun syncHistory(syncInfo: SyncAuthInfo) {
                Assert.fail()
            }

            override fun sendHistoryPing() {
                Assert.fail()
            }

            override fun syncBookmarks(syncInfo: SyncAuthInfo) {
                assembleBookmarksPing(SyncTelemetryPing(
                    version = 1,
                    uid = "abc123",
                    syncs = listOf(
                        SyncInfo(
                            at = now,
                            took = 5000,
                            engines = emptyList(),
                            failureReason = FailureReason(FailureName.Unknown, "Synergies not aligned")
                        )
                    ),
                    events = emptyList()
                ))
            }

            override fun sendBookmarksPing() {
                when (pingCount) {
                    0 -> BookmarksSync.apply {
                        Assert.assertEquals("Synergies not aligned", failureReason["other"].testGetValue())
                        Assert.assertFalse(failureReason["unexpected"].testHasValue())
                        Assert.assertFalse(failureReason["auth"].testHasValue())
                    }
                    else -> Assert.fail()
                }
                // We still need to send the ping, so that the counters are
                // cleared out between calls to `sendHistoryPing`.
                Pings.bookmarksSync.send()
                pingCount++
            }

            override fun close() {
                Assert.fail()
            }
        }
        val storage = TestablePlacesBookmarksStorage(conn)

        val result = storage.sync(SyncAuthInfo("kid", "token", 123L, "key", "serverUrl"))

        Assert.assertTrue(result is SyncStatus.Ok)
        Assert.assertEquals(1, conn.pingCount)
    }

    private fun BookmarkInfo.asBookmarkUpdateInfo(): BookmarkUpdateInfo = BookmarkUpdateInfo(this.parentGuid, this.position, this.title, this.url)
}
