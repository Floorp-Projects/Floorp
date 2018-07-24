/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.search

import android.graphics.Bitmap
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SearchEngineWriterTest {

    @Test
    fun buildSearchEngineXML() {
        val endSearchTermXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<OpenSearchDescription xmlns=\"http://a9.com/-/spec/opensearch/1.1/\">" +
            "<ShortName>testEngine</ShortName>" +
            "<Image height=\"16\" width=\"16\">" +
            "data:image/png;base64,Qml0bWFwICgyIHggMikgY29tcHJlc3NlZCBhcyBQTkcgd2l0aCBxdWFsaXR5IDEwMA==\n" +
            "</Image>" +
            "<Description>testEngine</Description>" +
            "<Url template=\"testEngine/?q={searchTerms}\" type=\"text/html\"/>" +
            "</OpenSearchDescription>"

        assertEquals(endSearchTermXml,
            SearchEngineWriter.buildSearchEngineXML("testEngine", "testEngine/?q=%s", createBitmap()))

        val middleSearchTermXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<OpenSearchDescription xmlns=\"http://a9.com/-/spec/opensearch/1.1/\">" +
            "<ShortName>testEngine</ShortName>" +
            "<Image height=\"16\" width=\"16\">" +
            "data:image/png;base64,Qml0bWFwICgyIHggMikgY29tcHJlc3NlZCBhcyBQTkcgd2l0aCBxdWFsaXR5IDEwMA==\n" +
            "</Image>" +
            "<Description>testEngine</Description>" +
            "<Url template=\"testEngine/?q={searchTerms}/tests\" type=\"text/html\"/>" +
            "</OpenSearchDescription>"

        assertEquals(middleSearchTermXml,
            SearchEngineWriter.buildSearchEngineXML("testEngine", "testEngine/?q=%s/tests", createBitmap()))
    }

    private fun createBitmap(): Bitmap {
        val width = 2
        val height = 2
        return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
    }
}
