package org.mozilla.fenix.helpers

import android.util.Log
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import okhttp3.mockwebserver.MockWebServer
import org.junit.Before
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.ui.robots.notificationShade

open class TestSetup {
    lateinit var mockWebServer: MockWebServer
    private val bookmarksStorage = PlacesBookmarksStorage(appContext.applicationContext)

    @Before
    fun setUp() {
        Log.i(TAG, "TestSetup: Starting the @Before setup")
        // Clear pre-existing notifications
        notificationShade {
            cancelAllShownNotifications()
        }
        runBlocking {
            // Reset locale to EN-US if needed.
            AppAndSystemHelper.resetSystemLocaleToEnUS()
            // Check and clear the downloads folder
            AppAndSystemHelper.clearDownloadsFolder()
            // Make sure the Wifi and Mobile Data connections are on
            AppAndSystemHelper.setNetworkEnabled(true)
            // Clear bookmarks left after a failed test
            val bookmarks = bookmarksStorage.getTree(BookmarkRoot.Mobile.id)?.children
            Log.i(TAG, "Before cleanup: Bookmarks storage contains: $bookmarks")
            bookmarks?.forEach {
                bookmarksStorage.deleteNode(it.guid)
                // TODO: Follow-up with a method to handle the DB update; the logs will still show the bookmarks in the storage before the test starts.
                Log.i(TAG, "After cleanup: Bookmarks storage contains: $bookmarks")
            }
        }
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
        }
        try {
            Log.i(TAG, "Try starting mockWebServer")
            mockWebServer.start()
        } catch (e: Exception) {
            Log.i(TAG, "Exception caught. Re-starting mockWebServer")
            mockWebServer.shutdown()
            mockWebServer.start()
        }
    }
}
