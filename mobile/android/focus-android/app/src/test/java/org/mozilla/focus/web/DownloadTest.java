/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.os.Environment;
import android.os.Parcel;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertEquals;

@RunWith(RobolectricTestRunner.class)
public class DownloadTest {
    private static final String fileName = "filename.png";
    @Test
    public void testGetters() {
        final Download download = new Download(
                "https://www.mozilla.org/image.png",
                "Focus/1.0",
                "Content-Disposition: attachment; filename=\"filename.png\"",
                "image/png",
                1024,
                Environment.DIRECTORY_DOWNLOADS,
                fileName);

        assertEquals("https://www.mozilla.org/image.png", download.getUrl());
        assertEquals("Focus/1.0", download.getUserAgent());
        assertEquals("Content-Disposition: attachment; filename=\"filename.png\"", download.getContentDisposition());
        assertEquals("image/png", download.getMimeType());
        assertEquals(1024, download.getContentLength());
        assertEquals(Environment.DIRECTORY_DOWNLOADS, download.getDestinationDirectory());
        assertEquals(fileName, download.getFileName());
    }

    @Test
    public void testParcelable() {
        final Parcel parcel = Parcel.obtain();

        {
            final Download download = new Download(
                    "https://www.mozilla.org/image.png",
                    "Focus/1.0",
                    "Content-Disposition: attachment; filename=\"filename.png\"",
                    "image/png",
                    1024,
                    Environment.DIRECTORY_PICTURES,
                    fileName);
            download.writeToParcel(parcel, 0);
        }

        parcel.setDataPosition(0);

        {
            final Download download = Download.CREATOR.createFromParcel(parcel);

            assertEquals("https://www.mozilla.org/image.png", download.getUrl());
            assertEquals("Focus/1.0", download.getUserAgent());
            assertEquals("Content-Disposition: attachment; filename=\"filename.png\"", download.getContentDisposition());
            assertEquals("image/png", download.getMimeType());
            assertEquals(1024, download.getContentLength());
            assertEquals(Environment.DIRECTORY_PICTURES, download.getDestinationDirectory());
            assertEquals(fileName, download.getFileName());
        }
    }
}