/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.equalTo
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.SessionPdfFileSaver

@RunWith(AndroidJUnit4::class)
@MediumTest
class PdfSaveTest : BaseSessionTest() {

    @Test fun savePdf() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
        mainSession.waitForPageStop()

        val result = sessionRule.waitForResult(mainSession.pdfFileSaver.save())
        assertThat("Check the pdf filename.", result.filename, equalTo(TRACEMONKEY_PDF_PATH.substringAfterLast("/")))

        val originalBytes = getTestBytes(TRACEMONKEY_PDF_PATH)
        assertThat("Check that bytes arrays are the same.", result.bytes, equalTo(originalBytes))

        assertFalse("Check private mode.", result.isPrivate)
    }

    @Test fun createResponseForSaving() {
        val bytes = byteArrayOf(1, 2, 3, 4, 5)
        val filename = "foobar.pdf"
        val url = "http://example.com/foobar.pdf"
        val response = SessionPdfFileSaver.createResponse(
            bytes,
            filename,
            url
        )!!

        assertThat("Uri", response.uri, equalTo("http://example.com/foobar.pdf"))
        assertThat("Data", response.body?.readBytes()!!, equalTo(bytes))
        assertThat("Content type", response.headers.get("content-type"), equalTo("application/pdf"))
        assertThat("Content length", response.headers.get("Content-Length")!!.toInt(), equalTo(bytes.size))
        assertThat("Filename", response.headers.get("Content-disposition"), equalTo("attachment; filename=\"" + filename + "\""))
    }
}
