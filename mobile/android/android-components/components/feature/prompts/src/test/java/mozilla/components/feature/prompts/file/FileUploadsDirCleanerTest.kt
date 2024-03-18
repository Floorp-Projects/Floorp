/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import mozilla.components.concept.engine.prompt.PromptRequest.File.Companion.DEFAULT_UPLOADS_DIR_NAME
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class FileUploadsDirCleanerTest {
    private lateinit var fileCleaner: FileUploadsDirCleaner

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        fileCleaner = FileUploadsDirCleaner(
            scope = MainScope(),
        ) {
            testContext.cacheDir
        }
        fileCleaner.fileNamesToBeDeleted = emptyList()
        fileCleaner.dispatcher = Dispatchers.Main
    }

    @Test
    fun `WHEN calling enqueueForCleanup THEN fileName should be added to fileNamesToBeDeleted list`() {
        val expectedFileName = "my_file.txt"
        fileCleaner.enqueueForCleanup(expectedFileName)
        assertEquals(expectedFileName, fileCleaner.fileNamesToBeDeleted.first())
    }

    @Test
    fun `WHEN calling cleanRecentUploads THEN all the enqueued files should be deleted and not enqueued files must be kept`() =
        runTestOnMain {
            val cachedDir = File(testContext.cacheDir, DEFAULT_UPLOADS_DIR_NAME)
            assertTrue(cachedDir.mkdir())

            val fileToBeDeleted = File(cachedDir, "my_file.txt")
            val fileToBeKept = File(cachedDir, "file_to_be_kept.txt")

            assertTrue(fileToBeDeleted.createNewFile())
            assertTrue(fileToBeKept.createNewFile())
            assertTrue(fileToBeDeleted.exists())
            assertTrue(fileToBeKept.exists())

            fileCleaner.enqueueForCleanup(fileToBeDeleted.name)

            fileCleaner.cleanRecentUploads()

            assertTrue(fileCleaner.fileNamesToBeDeleted.isEmpty())
            assertFalse(fileToBeDeleted.exists())
            assertTrue(fileToBeKept.exists())
        }

    @Test
    fun `WHEN calling cleanUploadsDirectory THEN the uploads directory should emptied`() =
        runTestOnMain {
            val cachedDir = File(testContext.cacheDir, DEFAULT_UPLOADS_DIR_NAME)
            assertTrue(cachedDir.mkdir())

            val fileToBeDeleted = File(cachedDir, "my_file.txt")

            assertTrue(fileToBeDeleted.createNewFile())
            assertTrue(fileToBeDeleted.exists())

            fileCleaner.cleanUploadsDirectory()

            assertFalse(fileToBeDeleted.exists())
            assertFalse(cachedDir.exists())
        }
}
