/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.downloads.DownloadItem
import java.io.File

@RunWith(FenixRobolectricTestRunner::class)
class ListTest {

    @Test
    fun `Test download in list but not on disk removed from list`() {
        val filePath1 = "filepath.txt"
        val filePath3 = "filepath3.txt"

        var file1 = File(filePath1)
        var file3 = File(filePath3)

        // Create files
        file1.createNewFile()
        file3.createNewFile()

        val item1 = DownloadItem(71, "filepath.txt", filePath1, "71 Mb", "Image/png")
        val item2 = DownloadItem(71, "filepath2.txt", "filepath2.txt", "71 Mb", "Image/png")
        val item3 = DownloadItem(71, "filepath3.txt", filePath3, "71 Mb", "Image/png")

        val testList = mutableListOf(item1, item2, item3)
        val comparisonList: MutableList<DownloadItem> = mutableListOf(item1, item3)

        val resultList = testList.filterNotExistsOnDisk()

        assertEquals(comparisonList, resultList)

        // Cleanup files
        file1.delete()
        file3.delete()
    }

    @Test
    fun `Test download in list and on disk remain in list`() {
        val filePath1 = "filepath.txt"
        val filePath2 = "filepath.txt"
        val filePath3 = "filepath3.txt"

        var file1 = File(filePath1)
        var file2 = File(filePath2)
        var file3 = File(filePath3)

        // Create files
        file1.createNewFile()
        file2.createNewFile()
        file3.createNewFile()

        val item1 = DownloadItem(71, "filepath.txt", filePath1, "71 Mb", "text/plain")
        val item2 = DownloadItem(71, "filepath2.txt", filePath2, "71 Mb", "text/plain")
        val item3 = DownloadItem(71, "filepath3.txt", filePath3, "71 Mb", "text/plain")

        val testList = mutableListOf(item1, item2, item3)
        val comparisonList: MutableList<DownloadItem> = mutableListOf(item1, item2, item3)

        val resultList = testList.filterNotExistsOnDisk()

        assertEquals(comparisonList, resultList)

        // Cleanup files
        file1.delete()
        file2.delete()
        file3.delete()
    }
}
