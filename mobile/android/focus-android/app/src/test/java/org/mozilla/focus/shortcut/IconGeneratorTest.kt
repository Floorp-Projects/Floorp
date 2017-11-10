/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.shortcut

import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

import org.junit.Assert.assertEquals

@RunWith(RobolectricTestRunner::class)
class IconGeneratorTest {
    @Test
    fun testRepresentativeCharacter() {
        assertEquals('M', IconGenerator.getRepresentativeCharacter("https://mozilla.org"))
        assertEquals('W', IconGenerator.getRepresentativeCharacter("http://wikipedia.org"))
        assertEquals('P', IconGenerator.getRepresentativeCharacter("http://plus.google.com"))
        assertEquals('E', IconGenerator.getRepresentativeCharacter("https://en.m.wikipedia.org/wiki/Main_Page"))

        // Stripping common prefixes
        assertEquals('T', IconGenerator.getRepresentativeCharacter("http://www.theverge.com"))
        assertEquals('F', IconGenerator.getRepresentativeCharacter("https://m.facebook.com"))
        assertEquals('T', IconGenerator.getRepresentativeCharacter("https://mobile.twitter.com"))

        // Special urls
        assertEquals('?', IconGenerator.getRepresentativeCharacter("file:///"))
        assertEquals('S', IconGenerator.getRepresentativeCharacter("file:///system/"))
        assertEquals('P', IconGenerator.getRepresentativeCharacter("ftp://people.mozilla.org/test"))

        // No values
        assertEquals('?', IconGenerator.getRepresentativeCharacter(""))
        assertEquals('?', IconGenerator.getRepresentativeCharacter(null))

        // Rubbish
        assertEquals('Z', IconGenerator.getRepresentativeCharacter("zZz"))
        assertEquals('Ö', IconGenerator.getRepresentativeCharacter("ölkfdpou3rkjaslfdköasdfo8"))
        assertEquals('?', IconGenerator.getRepresentativeCharacter("_*+*'##"))
        assertEquals('ツ', IconGenerator.getRepresentativeCharacter("¯\\_(ツ)_/¯"))
        assertEquals('ಠ', IconGenerator.getRepresentativeCharacter("ಠ_ಠ Look of Disapproval"))

        // Non-ASCII
        assertEquals('Ä', IconGenerator.getRepresentativeCharacter("http://www.ätzend.de"))
        assertEquals('名', IconGenerator.getRepresentativeCharacter("http://名がドメイン.com"))
        assertEquals('C', IconGenerator.getRepresentativeCharacter("http://√.com"))
        assertEquals('ß', IconGenerator.getRepresentativeCharacter("http://ß.de"))
        assertEquals('Ԛ', IconGenerator.getRepresentativeCharacter("http://ԛәлп.com/")) // cyrillic

        // Punycode
        assertEquals('X', IconGenerator.getRepresentativeCharacter("http://xn--tzend-fra.de")) // ätzend.de
        assertEquals('X', IconGenerator.getRepresentativeCharacter("http://xn--V8jxj3d1dzdz08w.com")) // 名がドメイン.com

        // Numbers
        assertEquals('1', IconGenerator.getRepresentativeCharacter("https://www.1and1.com/"))

        // IP
        assertEquals('1', IconGenerator.getRepresentativeCharacter("https://192.168.0.1"))
    }
}
