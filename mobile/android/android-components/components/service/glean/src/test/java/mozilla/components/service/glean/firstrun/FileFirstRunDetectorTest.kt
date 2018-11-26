/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.firstrun

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.io.File
import java.io.IOException

@RunWith(RobolectricTestRunner::class)
class FileFirstRunDetectorTest {
    private val dataDir: File =
        File(ApplicationProvider.getApplicationContext<Context>().applicationInfo.dataDir)

    @Before
    fun setUp() {
        // Delete any first run file, if it's there.
        val gleanFirstRunFile = File(dataDir, FileFirstRunDetector.FIRST_RUN_FILENAME)
        try {
            gleanFirstRunFile.delete()
        } catch (e: IOException) {
            // Do nothing, it's a test!
        }
    }

    @Test
    fun `"isFirstRun" returns true if no first run file exists`() {
        val detector = FileFirstRunDetector(gleanDataDir = dataDir)
        assertTrue(detector.isFirstRun())

        // Check that the file was created.
        val firstRunFile = File(dataDir, FileFirstRunDetector.FIRST_RUN_FILENAME)
        assertTrue(firstRunFile.exists())
        assertTrue(firstRunFile.isFile)

        // Check that calling isFirstRun again still returns true.
        assertTrue(detector.isFirstRun())
    }

    @Test
    fun `"isFirstRun" should return false if the first run file is a directory`() {
        // Make the first run file a directory.
        val gleanFirstRunFile = File(dataDir, FileFirstRunDetector.FIRST_RUN_FILENAME)
        gleanFirstRunFile.mkdirs()
        assertTrue(gleanFirstRunFile.exists())
        assertTrue(gleanFirstRunFile.isDirectory)

        // Check that "isFirstRun" returns false.
        val detector = FileFirstRunDetector(gleanDataDir = dataDir)
        assertFalse(detector.isFirstRun())
    }

    @Test
    fun `"reset" correctly deletes the first run file`() {
        // Check that "isFirstRun" returns true the first time.
        assertTrue(FileFirstRunDetector(gleanDataDir = dataDir).isFirstRun())

        // Then that it returns "false" the second time.
        val detector = FileFirstRunDetector(gleanDataDir = dataDir)
        assertFalse(detector.isFirstRun())
        detector.reset()

        val gleanFirstRunFile = File(dataDir, FileFirstRunDetector.FIRST_RUN_FILENAME)
        assertFalse(gleanFirstRunFile.exists())
        // Calling reset() twice should be a no-op.
        detector.reset()

        // It should return true again after a reset.
        assertTrue(FileFirstRunDetector(gleanDataDir = dataDir).isFirstRun())
    }
}
