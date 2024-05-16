/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.library.downloads.DownloadItem
import java.io.File

class ListTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `Test download in list but not on disk removed from list`() = runTest {
        val filePath1 = "filepath.txt"
        val filePath3 = "filepath3.txt"

        val file1 = File(filePath1)
        val file3 = File(filePath3)

        // Create files
        file1.createNewFile()
        file3.createNewFile()

        val item1 = DownloadItem(
            id = "71",
            url = "url",
            fileName = "filepath.txt",
            filePath = filePath1,
            formattedSize = "71 Mb",
            contentType = "Image/png",
            status = DownloadState.Status.COMPLETED,
        )
        val item2 = DownloadItem(
            id = "71",
            url = "url",
            fileName = "filepath2.txt",
            filePath = "filepath2.txt",
            formattedSize = "71 Mb",
            contentType = "Image/png",
            status = DownloadState.Status.COMPLETED,
        )
        val item3 = DownloadItem(
            id = "71",
            url = "url",
            fileName = "filepath3.txt",
            filePath = filePath3,
            formattedSize = "71 Mb",
            contentType = "Image/png",
            status = DownloadState.Status.COMPLETED,
        )

        val testList = mutableListOf(item1, item2, item3)
        val comparisonList: MutableList<DownloadItem> = mutableListOf(item1, item3)

        val resultList = testList.filterExistsOnDisk(coroutinesTestRule.testDispatcher)

        assertEquals(comparisonList, resultList)

        // Cleanup files
        file1.delete()
        file3.delete()
    }

    @Test
    fun `Test download in list and on disk remain in list`() = runTest {
        val filePath1 = "filepath.txt"
        val filePath2 = "filepath.txt"
        val filePath3 = "filepath3.txt"

        val file1 = File(filePath1)
        val file2 = File(filePath2)
        val file3 = File(filePath3)

        // Create files
        file1.createNewFile()
        file2.createNewFile()
        file3.createNewFile()

        val item1 = DownloadItem(
            id = "71",
            url = "url",
            fileName = "filepath.txt",
            filePath = filePath1,
            formattedSize = "71 Mb",
            contentType = "text/plain",
            status = DownloadState.Status.COMPLETED,
        )
        val item2 = DownloadItem(
            id = "72",
            url = "url",
            fileName = "filepath2.txt",
            filePath = filePath2,
            formattedSize = "71 Mb",
            contentType = "text/plain",
            status = DownloadState.Status.COMPLETED,
        )
        val item3 = DownloadItem(
            id = "73",
            url = "url",
            fileName = "filepath3.txt",
            filePath = filePath3,
            formattedSize = "71 Mb",
            contentType = "text/plain",
            status = DownloadState.Status.COMPLETED,
        )

        val testList = mutableListOf(item1, item2, item3)
        val comparisonList: MutableList<DownloadItem> = mutableListOf(item1, item2, item3)

        val resultList = testList.filterExistsOnDisk(coroutinesTestRule.testDispatcher)

        assertEquals(comparisonList, resultList)

        // Cleanup files
        file1.delete()
        file2.delete()
        file3.delete()
    }
}
