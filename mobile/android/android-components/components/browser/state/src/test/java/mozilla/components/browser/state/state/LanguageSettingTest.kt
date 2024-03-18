/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.translate.LanguageSetting.ALWAYS
import mozilla.components.concept.engine.translate.LanguageSetting.NEVER
import mozilla.components.concept.engine.translate.LanguageSetting.OFFER
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class LanguageSettingTest {
    @Test
    fun `GIVEN an OFFER LanguageSetting THEN find its Boolean counterpart`() {
        val isAlways = OFFER.toBoolean(categoryToSetFor = ALWAYS)
        assertFalse(isAlways!!)

        val isOffer = OFFER.toBoolean(categoryToSetFor = OFFER)
        assertTrue(isOffer!!)

        val isNever = OFFER.toBoolean(categoryToSetFor = NEVER)
        assertFalse(isNever!!)
    }

    @Test
    fun `GIVEN an ALWAYS LanguageSetting THEN find its Boolean counterpart`() {
        val isAlways = ALWAYS.toBoolean(categoryToSetFor = ALWAYS)
        assertTrue(isAlways!!)

        val isOffer = ALWAYS.toBoolean(categoryToSetFor = OFFER)
        assertNull(isOffer)

        val isNever = ALWAYS.toBoolean(categoryToSetFor = NEVER)
        assertFalse(isNever!!)
    }

    @Test
    fun `GIVEN a NEVER LanguageSetting THEN find its Boolean counterpart`() {
        val isAlways = NEVER.toBoolean(categoryToSetFor = ALWAYS)
        assertFalse(isAlways!!)

        val isOffer = NEVER.toBoolean(categoryToSetFor = OFFER)
        assertNull(isOffer)

        val isNever = NEVER.toBoolean(categoryToSetFor = NEVER)
        assertTrue(isNever!!)
    }

    @Test
    fun `GIVEN a Boolean corresponding to Always THEN find its LanguageSetting counterpart`() {
        var isAlways = true
        var conversion = ALWAYS.toLanguageSetting(value = isAlways)
        assertEquals(conversion, ALWAYS)

        isAlways = false
        conversion = ALWAYS.toLanguageSetting(value = isAlways)
        assertEquals(conversion, OFFER)
    }

    @Test
    fun `GIVEN a Boolean corresponding to Never THEN find its LanguageSetting counterpart`() {
        var isNever = true
        var conversion = NEVER.toLanguageSetting(value = isNever)
        assertEquals(conversion, NEVER)

        isNever = false
        conversion = NEVER.toLanguageSetting(value = isNever)
        assertEquals(conversion, OFFER)
    }

    @Test
    fun `GIVEN a Boolean corresponding to Offer THEN find its LanguageSetting counterpart`() {
        var isOffer = true
        var conversion = OFFER.toLanguageSetting(value = isOffer)
        assertEquals(conversion, OFFER)

        isOffer = false
        conversion = OFFER.toLanguageSetting(value = isOffer)
        assertNull(conversion)
    }
}
