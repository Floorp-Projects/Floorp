/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentBuilder;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.robolectric.RuntimeEnvironment;

import java.io.File;
import java.util.Collections;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

/**
 * VerifyAction: Validate downloaded content. Does it still exist and does it have the correct checksum?
 */
@RunWith(TestRunner.class)
public class TestVerifyAction {
    /**
     * Scenario: Downloaded file does not exist anymore.
     *
     * Verify that:
     *  * Content is re-scheduled for download.
     */
    @Test
    public void testReschedulingIfFileDoesNotExist() throws Exception {
        DownloadContent content = new DownloadContentBuilder().build();
        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        when(catalog.getDownloadedContent()).thenReturn(Collections.singletonList(content));

        File file = mock(File.class);
        when(file.exists()).thenReturn(false);

        VerifyAction action = spy(new VerifyAction());
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).scheduleDownload(content);
    }

    /**
     * Scenario: Content has been scheduled for download.
     *
     * Verify that:
     *  * Download action is started
     */
    @Test
    public void testStartingDownloadsAfterScheduling() {
        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        when(catalog.hasScheduledDownloads()).thenReturn(true);

        VerifyAction action = spy(new VerifyAction());
        action.perform(RuntimeEnvironment.application, catalog);

        verify(action).startDownloads(any(Context.class));
    }

    /**
     * Scenario: Checksum of existing file does not match expectation.
     *
     * Verify that:
     *  * Content is re-scheduled for download.
     */
    @Test
    public void testReschedulingIfVerificationFailed() throws Exception {
        DownloadContent content = new DownloadContentBuilder().build();
        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        when(catalog.getDownloadedContent()).thenReturn(Collections.singletonList(content));

        File file = mock(File.class);
        when(file.exists()).thenReturn(true);

        VerifyAction action = spy(new VerifyAction());
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);
        doReturn(false).when(action).verify(eq(file), anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).scheduleDownload(content);
    }

    /**
     * Scenario: Downloaded file exists and has the correct checksum.
     *
     * Verify that:
     *  * No download is scheduled
     *  * Download action is not started
     */
    @Test
    public void testSuccessfulVerification() throws Exception {
        DownloadContent content = new DownloadContentBuilder().build();
        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        when(catalog.getDownloadedContent()).thenReturn(Collections.singletonList(content));

        File file = mock(File.class);
        when(file.exists()).thenReturn(true);

        VerifyAction action = spy(new VerifyAction());
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);
        doReturn(true).when(action).verify(eq(file), anyString());

        verify(catalog, never()).scheduleDownload(content);
        verify(action, never()).startDownloads(RuntimeEnvironment.application);
    }
}
