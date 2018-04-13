/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import mozilla.components.browser.session.helper.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class ObservableListTest {
    @Test
    fun `size property returns size of list`() {
        val list = ObservableList<String>()
        assertEquals(0, list.size)

        list.add("Hello")
        list.add("World")

        assertEquals(2, list.size)

        list.remove("Hello")

        assertEquals(1, list.size)
    }

    @Test
    fun `observer is called when item is added`() {
        val list = ObservableList<String>()

        val observer: ObservableList.Observer<String> = mock()
        list.register(observer)

        list.add("Hello")
        list.add("World")

        verify(observer).onValueAdded("Hello")
        verify(observer).onValueAdded("World")
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is called when item is removed`() {
        val list = ObservableList<String>()

        list.add("Hello")
        list.add("World")

        val observer: ObservableList.Observer<String> = mock()
        list.register(observer)

        list.remove("World")

        verify(observer).onValueRemoved("World")
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `observer is not called when item to remove is not in list`() {
        val list = ObservableList<String>()

        list.add("Hello")
        list.add("World")

        val observer: ObservableList.Observer<String> = mock()
        list.register(observer)

        list.remove("Banana")

        verify(observer, never()).onValueRemoved(anyString())
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `list returns selected item`() {
        val list = ObservableList<String>()

        list.add("banana")
        list.add("apple")
        list.add("orange")

        list.select("banana")

        assertEquals("banana", list.selected)

        list.select("apple")

        assertEquals("apple", list.selected)
    }

    @Test
    fun `observer is not called after unregistering`() {
        val list = ObservableList<String>()

        val observer: ObservableList.Observer<String> = mock()
        list.register(observer)

        list.add("Hello")
        verify(observer).onValueAdded("Hello")

        list.unregister(observer)

        list.add("World")
        list.remove("Hello")

        verify(observer, never()).onValueAdded("World")
        verify(observer, never()).onValueRemoved("Hello")
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `list allows add and select with one method call`() {
        val list = ObservableList<String>()

        list.add("banana")
        list.select("banana")

        assertEquals("banana", list.selected)

        list.add("apple")
        assertEquals("banana", list.selected)

        list.add("orange", select = true)
        assertEquals("orange", list.selected)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `selecting item that is not in list will throw exception`() {
        val list = ObservableList<String>()

        list.add("orange")
        list.select("banana")
    }

    @Test
    fun `observer will be notified when item is selected`() {
        val list = ObservableList<String>()
        list.add("orange")

        val observer: ObservableList.Observer<String> = mock()
        list.register(observer)

        list.select("orange")

        verify(observer).onValueSelected("orange")
        verifyNoMoreInteractions(observer)
    }

    @Test
    fun `initial items and selection passed to constructor will be used`() {
        val list = ObservableList<String>(
                initialValues = listOf("banana", "orange"),
                selectedIndex = 1)

        assertEquals(2, list.size)
        assertEquals("orange", list.selected)
    }
}
