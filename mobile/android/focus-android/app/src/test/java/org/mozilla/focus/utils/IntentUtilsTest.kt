/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.utils

import android.app.PendingIntent
import android.os.Build
import org.junit.Assert
import org.junit.Test
import java.lang.reflect.Field
import java.lang.reflect.Modifier

class IntentUtilsTest {

    @Test
    fun `given a build version lower than 23, when defaultIntentPendingFlags is called, then flag 0 should be returned`() {
        setFinalStaticValue(Build.VERSION::class.java.getField("SDK_INT"), 22)

        Assert.assertEquals(0, IntentUtils.defaultIntentPendingFlags)
    }

    @Test
    fun `given a build version bigger than 22, when defaultIntentPendingFlags is called, then flag FLAG_IMMUTABLE should be returned`() {
        setFinalStaticValue(Build.VERSION::class.java.getField("SDK_INT"), 23)
        Assert.assertEquals(PendingIntent.FLAG_IMMUTABLE, IntentUtils.defaultIntentPendingFlags)
    }

    @Throws(Exception::class)
    fun setFinalStaticValue(field: Field, newValue: Any) {
        field.isAccessible = true

        val modifiersField = Field::class.java.getDeclaredField("modifiers")
        modifiersField.isAccessible = true
        modifiersField.setInt(field, field.modifiers and Modifier.FINAL.inv())

        field.set(null, newValue)
    }
}
