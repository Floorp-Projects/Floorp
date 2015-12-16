/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.robolectric.RuntimeEnvironment;

import java.io.File;
import java.util.Collections;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.StatusLine;
import ch.boye.httpclientandroidlib.client.HttpClient;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

/**
 * DownloadAction: Download content that has been scheduled during "study" or "verify".
 */
@RunWith(TestRunner.class)
public class TestDownloadAction {
    private static final String TEST_URL = "http://example.org";

    /**
     * Scenario: The current network is metered.
     *
     * Verify that:
     *  * No download is performed on a metered network
     */
    @Test
    public void testNothingIsDoneOnMeteredNetwork() throws Exception {
        DownloadAction action = spy(new DownloadAction(null));
        doReturn(true).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        action.perform(RuntimeEnvironment.application, null);

        verify(action, never()).buildHttpClient();
        verify(action, never()).download(any(HttpClient.class), anyString(), any(File.class));
    }

    /**
     * Scenario: Content is scheduled for download but already exists locally (with correct checksum).
     *
     * Verify that:
     *  * No download is performed for existing file
     *  * Content is marked as downloaded in the catalog
     */
    @Test
    public void testExistingAndVerifiedFilesAreNotDownloadedAgain() throws Exception {
        DownloadContent content = new DownloadContent.Builder().build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File file = mock(File.class);
        doReturn(true).when(file).exists();
        doReturn(file).when(action).createTemporaryFile(RuntimeEnvironment.application, content);
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);
        doReturn(true).when(action).verify(eq(file), anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        verify(action, never()).download(any(HttpClient.class), anyString(), any(File.class));
        verify(catalog).markAsDownloaded(content);
    }

    /**
     * Scenario: Server returns a server error (HTTP 500).
     *
     * Verify that:
     *  * Situation is treated as recoverable (RecoverableDownloadContentException)
     */
    @Test(expected=BaseAction.RecoverableDownloadContentException.class)
    public void testServerErrorsAreRecoverable() throws Exception {
        StatusLine status = mock(StatusLine.class);
        doReturn(500).when(status).getStatusCode();

        HttpResponse response = mock(HttpResponse.class);
        doReturn(status).when(response).getStatusLine();

        HttpClient client = mock(HttpClient.class);
        doReturn(response).when(client).execute(any(HttpUriRequest.class));

        DownloadAction action = spy(new DownloadAction(null));
        action.download(client, TEST_URL, null);

        verify(client).execute(any(HttpUriRequest.class));
    }

    /**
     * Scenario: Server returns a client error (HTTP 404).
     *
     * Verify that:
     *  * Situation is treated as unrecoverable (UnrecoverableDownloadContentException)
     */
    @Test(expected=BaseAction.UnrecoverableDownloadContentException.class)
    public void testClientErrorsAreUnrecoverable() throws Exception {
        StatusLine status = mock(StatusLine.class);
        doReturn(404).when(status).getStatusCode();

        HttpResponse response = mock(HttpResponse.class);
        doReturn(status).when(response).getStatusLine();

        HttpClient client = mock(HttpClient.class);
        doReturn(response).when(client).execute(any(HttpUriRequest.class));

        DownloadAction action = spy(new DownloadAction(null));
        action.download(client, TEST_URL, null);

        verify(client).execute(any(HttpUriRequest.class));
    }

    /**
     * Scenario: A successful download has been performed.
     *
     * Verify that:
     *  * The content will be extracted to the destination
     *  * The content is marked as downloaded in the catalog
     */
    @Test
    public void testSuccessfulDownloadsAreMarkedAsDownloaded() throws Exception {
        DownloadContent content = new DownloadContent.Builder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File file = mock(File.class);
        doReturn(false).when(file).exists();
        doReturn(file).when(action).createTemporaryFile(RuntimeEnvironment.application, content);
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        doReturn(false).when(action).verify(eq(file), anyString());
        doNothing().when(action).download(any(HttpClient.class), anyString(), eq(file));
        doReturn(true).when(action).verify(eq(file), anyString());
        doNothing().when(action).extract(eq(file), eq(file), anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        verify(action).buildHttpClient();
        verify(action).download(any(HttpClient.class), anyString(), eq(file));
        verify(action).extract(eq(file), eq(file), anyString());
        verify(catalog).markAsDownloaded(content);
    }
}
