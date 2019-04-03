/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.Dispatchers
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.resetGlean
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.io.File
import java.io.FileReader
import java.io.BufferedReader
import java.io.FileOutputStream
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
class PingStorageEngineTest {
    // Filenames and paths to test with, regenerated with each test.
    private lateinit var fileName1: UUID
    private lateinit var path1: String
    private lateinit var fileName2: UUID
    private lateinit var path2: String
    private lateinit var fileName3: UUID
    private lateinit var path3: String

    private lateinit var fileNames: Array<String>
    private lateinit var pathNames: Array<String>

    private lateinit var pingStorageEngine: PingStorageEngine

    @Before
    fun setup() {
        resetGlean()
        pingStorageEngine = Glean.pingStorageEngine

        // Clear out pings and assert that there are none in the directory before we start
        pingStorageEngine.storageDirectory.listFiles()?.forEach { file ->
            file.delete()
        }
        assertNull("Pending pings directory must be empty before test start",
            pingStorageEngine.storageDirectory.listFiles())

        fileName1 = UUID.randomUUID()
        fileName2 = UUID.randomUUID()
        fileName3 = UUID.randomUUID()

        path1 = "/test/$fileName1"
        path2 = "/test/$fileName2"
        path3 = "/test/$fileName3"

        fileNames = arrayOf(fileName1.toString(), fileName2.toString(), fileName3.toString())
        pathNames = arrayOf(path1, path2, path3)
    }

    @Test
    fun `storage engine correctly stores pings to file`() {
        runBlocking(Dispatchers.IO) {
            pingStorageEngine.store(fileName1, path1, "dummy data").join()
        }

        assertEquals(1, pingStorageEngine.storageDirectory.listFiles()?.count())

        val file = File(pingStorageEngine.storageDirectory, fileName1.toString())
        BufferedReader(FileReader(file)).use {
            val lines = it.readLines()
            assertEquals(path1, lines[0])
            assertEquals("dummy data", lines[1])
        }
    }

    @Test
    fun `process correctly processes files`() {
        runBlocking(Dispatchers.IO) {
            pingStorageEngine.store(fileName1, path1, "dummy data").join()
        }

        assertEquals(1, pingStorageEngine.storageDirectory.listFiles()?.count())
        assertTrue(pingStorageEngine.process(this::testCallback))
    }

    @Test
    fun `process correctly handles callback returning false`() {
        runBlocking(Dispatchers.IO) {
            pingStorageEngine.store(fileName1, path1, "dummy data").join()
        }

        assertEquals(1, pingStorageEngine.storageDirectory.listFiles()?.count())
        assertFalse(pingStorageEngine.process(this::testFailedCallback))
    }

    private fun testCallback(path: String, pingData: String, config: Configuration): Boolean {
        assertTrue(pathNames.contains(path))
        assertEquals("dummy data", pingData)
        assertEquals(Glean.configuration, config)

        return true
    }

    private fun testFailedCallback(path: String, pingData: String, config: Configuration): Boolean {
        assertTrue(pathNames.contains(path))
        assertEquals("dummy data", pingData)
        assertEquals(Glean.configuration, config)
        return false
    }

    @Test
    fun `listPingFiles correctly lists all files`() {
        runBlocking(Dispatchers.IO) {
            pingStorageEngine.store(fileName1, path1, "dummy data").join()
            pingStorageEngine.store(fileName2, path2, "dummy data").join()
            pingStorageEngine.store(fileName3, path3, "dummy data").join()
        }

        val files = pingStorageEngine.storageDirectory.listFiles()
        assertEquals(3, files?.count())

        files?.forEach { file ->
            assertTrue(fileNames.contains(file.name))
        }
    }

    @Test
    fun `process removes files that do not have UUID names`() {
        // Clear out pings and assert that there are none in the directory before we start
        pingStorageEngine.storageDirectory.listFiles()?.forEach { file ->
            file.delete()
        }
        assertNull("Pending pings directory must be empty before test start",
            pingStorageEngine.storageDirectory.listFiles())

        // Store a "valid" ping with a UUID file name, using runBlocking to make sure the write is
        // complete before continuing.
        runBlocking {
            pingStorageEngine.store(fileName1, path1, "dummy data").join()
        }

        // Store an "invalid" ping without a UUID file name (writing synchronously on purpose as we
        // immediately assert the file count after this)
        val invalidPingFile = File(pingStorageEngine.storageDirectory, "NotUuid")
        FileOutputStream(invalidPingFile, true).bufferedWriter().use {
            it.write("/test/NotUuid")
            it.newLine()
            it.write("dummy data")
            it.newLine()
            it.flush()
        }

        // Check to see that we list both the valid and invalid ping files
        assertEquals(2, pingStorageEngine.storageDirectory.listFiles()?.count())

        var numCalls = 0
        fun testCountingCallback(path: String, pingData: String, config: Configuration): Boolean {
            numCalls++
            return testCallback(path, pingData, config)
        }

        //  Process the directory
        assertTrue(pingStorageEngine.process(::testCountingCallback))
        assertEquals("The callback should be called once", 1, numCalls)

        // Check to see that we list no ping files and no invalid files
        assertEquals(0, pingStorageEngine.storageDirectory.listFiles()?.count())
    }
}
