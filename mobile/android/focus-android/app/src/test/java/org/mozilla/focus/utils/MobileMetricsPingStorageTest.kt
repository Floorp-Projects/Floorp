package org.mozilla.focus.utils

import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.FocusApplication
import org.robolectric.RobolectricTestRunner
import java.io.File

@RunWith(RobolectricTestRunner::class)
class MobileMetricsPingStorageTest {
    private val file = File("${(ApplicationProvider.getApplicationContext() as FocusApplication).cacheDir}/${MobileMetricsPingStorage.STORAGE_FOLDER}/${MobileMetricsPingStorage.FILE_NAME}")
    private var storage = MobileMetricsPingStorage(ApplicationProvider.getApplicationContext(), file)

    @Before
    fun init() {
        file.delete()
    }

    @Test
    fun testShouldStoreMetricsIfCacheIsMissing() {
        Assert.assertTrue(storage.shouldStoreMetrics())
    }

    @Test
    fun testShouldntStoreMetricsIfCacheExists() {
        runBlocking { storage.save(JSONObject()) }

        Assert.assertFalse(storage.shouldStoreMetrics())
    }

    @Test
    fun testClearStorageRemovesTheCachedFile() {
        runBlocking {
            storage.save(JSONObject())
            Assert.assertTrue(file.exists())

            storage.clearStorage()
            Assert.assertFalse(file.exists())
        }
    }

    @Test
    fun testSavingAndLoadingAFile() {
        runBlocking {
            val jsonData = "{\"app\":\"Focus\"}"
            storage.save(JSONObject(jsonData))
            val jsonObject = storage.load()

            Assert.assertEquals(jsonData, jsonObject.toString())
        }
    }
}
