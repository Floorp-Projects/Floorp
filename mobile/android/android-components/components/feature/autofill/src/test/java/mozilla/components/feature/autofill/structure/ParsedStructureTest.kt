/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.structure

import android.os.Parcel
import android.view.autofill.AutofillId
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ParsedStructureTest {
    @Test
    fun `Given a ParsedStructure WHEN parcelling and unparcelling it THEN get the same object`() {
        // AutofillId constructor is private but it can be constructed from a parcel.
        // Use this route instead of mocking to avoid errors like below:
        // org.robolectric.shadows.ShadowParcel$UnreliableBehaviorError: Looking for Integer at position 72, found String
        val usernameIdAutofillIdParcel = Parcel.obtain().apply {
            writeInt(1) // viewId
            writeInt(3) // flags
            writeInt(78) // virtualIntId
            setDataPosition(0) // be a good citizen
        }
        val passwordIdAutofillParcel = Parcel.obtain().apply {
            writeInt(11) // viewId
            writeInt(31) // flags
            writeInt(781) // virtualIntId
            setDataPosition(0) // be a good citizen
        }
        val parsedStructure = ParsedStructure(
            usernameId = AutofillId.CREATOR.createFromParcel(usernameIdAutofillIdParcel),
            passwordId = AutofillId.CREATOR.createFromParcel(passwordIdAutofillParcel),
            packageName = "test",
            webDomain = "https://mozilla.org",
        )

        // Write the object in a new Parcel.
        val parcel = Parcel.obtain()
        parsedStructure.writeToParcel(parcel, 0)

        // Reset Parcel r/w position to be read from beginning afterwards.
        parcel.setDataPosition(0)

        // Reconstruct the original object from the Parcel.
        val result = ParsedStructure(parcel)

        assertEquals(parsedStructure, result)
    }
}
