/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.lang.Thread.sleep

@RunWith(AndroidJUnit4::class)
class BreadcrumbPriorityQueueTest {

    @Test
    fun `Breadcrumb priority queue stores only max number of breadcrumbs`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER
        var crashBreadcrumbs = BreadcrumbPriorityQueue(5)
        repeat(10) {
            crashBreadcrumbs.add(Breadcrumb(testMessage, testData, testCategory, testLevel, testType))
        }
        assertEquals(crashBreadcrumbs.size, 5)

        crashBreadcrumbs = BreadcrumbPriorityQueue(10)
        repeat(15) {
            crashBreadcrumbs.add(Breadcrumb(testMessage, testData, testCategory, testLevel, testType))
        }
        assertEquals(crashBreadcrumbs.size, 10)
    }

    @Test
    fun `Breadcrumb priority queue stores the correct priority`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testType = Breadcrumb.Type.USER
        val maxNum = 5
        var crashBreadcrumbs = BreadcrumbPriorityQueue(maxNum)

        repeat(maxNum) {
            crashBreadcrumbs.add(
                    Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.DEBUG, testType)
            )
        }
        assertEquals(crashBreadcrumbs.size, maxNum)

        repeat(maxNum) {
            crashBreadcrumbs.add(
                    Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.INFO, testType)
            )
        }

        for (i in 0 until maxNum) {
            assertEquals(crashBreadcrumbs.elementAt(i).level, Breadcrumb.Level.INFO)
        }

        repeat(maxNum) {
            crashBreadcrumbs.add(
                    Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.DEBUG, testType)
            )
        }

        for (i in 0 until maxNum) {
            assertEquals(crashBreadcrumbs.elementAt(i).level, Breadcrumb.Level.INFO)
        }

        repeat(maxNum) {
            crashBreadcrumbs.add(
                    Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.WARNING, testType)
            )
        }

        for (i in 0 until maxNum) {
            assertEquals(crashBreadcrumbs.elementAt(i).level, Breadcrumb.Level.WARNING)
        }
    }

    @Test
    fun `Breadcrumb priority queue output list result is sorted by time`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testType = Breadcrumb.Type.USER
        val maxNum = 10
        var crashBreadcrumbs = BreadcrumbPriorityQueue(maxNum)

        repeat(maxNum) {
            crashBreadcrumbs.add(
                    Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.DEBUG, testType)
            )
            sleep(100) /* make sure time elapsed */
        }

        var result = crashBreadcrumbs.toSortedArrayList()
        var time = result[0].date
        for (i in 1 until result.size) {
            assertTrue(time.before(result[i].date))
            time = result[i].date
        }

        repeat(maxNum / 2) {
            crashBreadcrumbs.add(
                    Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.INFO, testType)
            )
            sleep(100) /* make sure time elapsed */
        }

        result = crashBreadcrumbs.toSortedArrayList()
        time = result[0].date
        for (i in 1 until result.size) {
            assertTrue(time.before(result[i].date))
            time = result[i].date
        }
    }
}
