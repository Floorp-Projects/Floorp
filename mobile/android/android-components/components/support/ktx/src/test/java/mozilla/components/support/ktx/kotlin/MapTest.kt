/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import android.os.Binder
import android.os.Bundle
import android.os.Parcelable
import android.util.Size
import android.util.SizeF
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import java.io.Serializable

@RunWith(RobolectricTestRunner::class)
class MapTest {

    @Test
    fun testToBundle() {
        val binder = mock(Binder::class.java)
        val boolean = true
        val booleanArray = BooleanArray(2, { true })
        val bundle = mock(Bundle::class.java)
        val byte = Byte.MAX_VALUE
        val byteArray = ByteArray(2, { Byte.MAX_VALUE })
        val char = Char.MAX_HIGH_SURROGATE
        val charArray = CharArray(2, { 'c' })
        val charSequence = "abc"
        val double = Double.MAX_VALUE
        val doubleArray = DoubleArray(2, { Double.MAX_VALUE })
        val float = Float.MAX_VALUE
        val floatArray = FloatArray(2, { Float.MAX_VALUE })
        val int = Int.MAX_VALUE
        val intArray = IntArray(2, { Int.MAX_VALUE })
        val long = Long.MAX_VALUE
        val longArray = LongArray(2, { Long.MAX_VALUE })
        val short = Short.MAX_VALUE
        val shortArray = ShortArray(2, { Short.MAX_VALUE })
        val size = mock(Size::class.java)
        val sizeF = mock(SizeF::class.java)
        val parcelable = mock(Parcelable::class.java)
        val serializable = mock(Serializable::class.java)

        val map = mapOf<String, Any>(
                "binder" to binder,
                "boolean" to boolean,
                "booleanArray" to booleanArray,
                "bundle" to bundle,
                "byte" to byte,
                "byteArray" to byteArray,
                "char" to char,
                "charArray" to charArray,
                "charSequence" to charSequence,
                "double" to double,
                "doubleArray" to doubleArray,
                "float" to float,
                "floatArray" to floatArray,
                "int" to int,
                "intArray" to intArray,
                "long" to long,
                "longArray" to longArray,
                "short" to short,
                "shortArray" to shortArray,
                "size" to size,
                "sizeF" to sizeF,
                "parcelable" to parcelable,
                "serializable" to serializable
        )

        val resultBundle = map.toBundle()
        assertEquals(binder, resultBundle.getBinder("binder"))
        assertEquals(boolean, resultBundle.getBoolean("boolean"))
        assertEquals(booleanArray, resultBundle.getBooleanArray("booleanArray"))
        assertEquals(bundle, resultBundle.getBundle("bundle"))
        assertEquals(byte, resultBundle.getByte("byte"))
        assertEquals(byteArray, resultBundle.getByteArray("byteArray"))
        assertEquals(char, resultBundle.getChar("char"))
        assertEquals(charArray, resultBundle.getCharArray("charArray"))
        assertEquals(charSequence, resultBundle.getCharSequence("charSequence"))
        assertEquals(double, resultBundle.getDouble("double"), Double.MIN_VALUE)
        assertEquals(doubleArray, resultBundle.getDoubleArray("doubleArray"))
        assertEquals(float, resultBundle.getFloat("float"))
        assertEquals(floatArray, resultBundle.getFloatArray("floatArray"))
        assertEquals(int, resultBundle.getInt("int"))
        assertEquals(intArray, resultBundle.getIntArray("intArray"))
        assertEquals(long, resultBundle.getLong("long"))
        assertEquals(longArray, resultBundle.getLongArray("longArray"))
        assertEquals(short, resultBundle.getShort("short"))
        assertEquals(shortArray, resultBundle.getShortArray("shortArray"))
        assertEquals(size, resultBundle.getSize("size"))
        assertEquals(sizeF, resultBundle.getSizeF("sizeF"))
        assertEquals(parcelable, resultBundle.getParcelable("parcelable"))
        assertEquals(serializable, resultBundle.getSerializable("serializable"))
    }
}
