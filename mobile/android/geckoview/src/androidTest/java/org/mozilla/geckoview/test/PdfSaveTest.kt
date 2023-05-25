/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.equalTo
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class PdfSaveTest : BaseSessionTest() {

    @Test fun savePdf() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
        mainSession.waitForPageStop()

        val response = sessionRule.waitForResult(mainSession.pdfFileSaver.save())
        val originalBytes = getTestBytes(TRACEMONKEY_PDF_PATH)
        val filename = TRACEMONKEY_PDF_PATH.substringAfterLast("/")

        assertThat("Check the response uri.", response.uri.substringAfterLast("/"), equalTo(filename))
        assertThat("Check the response content-type.", response.headers.get("content-type"), equalTo("application/pdf"))
        assertThat("Check the response filename.", response.headers.get("Content-disposition"), equalTo("attachment; filename=\"" + filename + "\""))
        assertThat("Check that bytes arrays are the same.", response.body?.readBytes(), equalTo(originalBytes))
    }
}
