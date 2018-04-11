/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.session

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito
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

    inline fun <reified T : Any> mock(): T = Mockito.mock(T::class.java)!!
}