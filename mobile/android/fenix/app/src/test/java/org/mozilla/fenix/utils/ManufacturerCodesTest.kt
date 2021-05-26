/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.os.Build
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.lang.reflect.Modifier

class ManufacturerCodesTest {

    private val manufacturerField = Build::class.java.getDeclaredField("MANUFACTURER")
    private var manufacturer: String?
        get() = Build.MANUFACTURER
        set(value) { manufacturerField.set(null, value) }

    @Before
    fun setUp() {
        enableManufacturerModifications()
        manufacturer = null // reset to default state before test.
        assertEquals(null, Build.MANUFACTURER) // sanity check.
    }

    private fun enableManufacturerModifications() {
        // Mocking, which might be simpler, doesn't seem to work so we use reflection.
        // Methodology via https://stackoverflow.com/a/3301720/2219998
        manufacturerField.isAccessible = true
        val modifiers = manufacturerField.javaClass.getDeclaredField("modifiers")
        modifiers.isAccessible = true
        modifiers.setInt(manufacturerField, manufacturerField.modifiers and Modifier.FINAL.inv())
    }

    @After
    fun tearDown() {
        // After this method, Build.MANUFACTURER appears to return to
        // static final so we don't need to undo that.
        manufacturer = null
        assertEquals(null, Build.MANUFACTURER) // sanity check.
    }

    @Test // To reduce boilerplate, we avoid best practice and put several tests in one.
    fun testIsLG() {
        manufacturer = "LGE" // expected value for lg devices
        assertTrue(ManufacturerCodes.isLG)

        manufacturer = "lge" // unexpected value but is still an lg device
        assertTrue(ManufacturerCodes.isLG)

        manufacturer = "samsung"
        assertFalse(ManufacturerCodes.isLG)
    }

    @Test // To reduce boilerplate, we avoid best practice and put several tests in one.
    fun testIsSamsung() {
        manufacturer = "samsung" // expected value for samsung devices
        assertTrue(ManufacturerCodes.isSamsung)

        manufacturer = "SaMsUnG" // unexpected value but is still a samsung device
        assertTrue(ManufacturerCodes.isSamsung)

        manufacturer = "LGE"
        assertFalse(ManufacturerCodes.isSamsung)
    }
}
