package org.mozilla.focus.utils

import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assert
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.io.File

@RunWith(RobolectricTestRunner::class)
class MobileMetricsPingStorageTest {
    private val file = File("${RuntimeEnvironment.application.cacheDir}/${MobileMetricsPingStorage.STORAGE_FOLDER}/${MobileMetricsPingStorage.FILE_NAME}")
    private var storage = MobileMetricsPingStorage(RuntimeEnvironment.application, file)

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
