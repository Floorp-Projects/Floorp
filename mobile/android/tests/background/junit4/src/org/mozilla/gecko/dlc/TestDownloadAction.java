/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;

import android.content.Context;

import org.robolectric.RuntimeEnvironment;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.util.Collections;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpStatus;
import ch.boye.httpclientandroidlib.StatusLine;
import ch.boye.httpclientandroidlib.client.HttpClient;
import ch.boye.httpclientandroidlib.client.methods.HttpGet;
import ch.boye.httpclientandroidlib.client.methods.HttpUriRequest;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
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
     * Scenario: No (connected) network is available.
     *
     * Verify that:
     *  * No download is performed
     */
    @Test
    public void testNothingIsDoneIfNoNetworkIsAvailable() throws Exception {
        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isConnectedToNetwork(RuntimeEnvironment.application);

        action.perform(RuntimeEnvironment.application, null);

        verify(action, never()).isActiveNetworkMetered(any(Context.class));
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
        HttpClient client = mockHttpClient(500, "");

        File temporaryFile = mock(File.class);
        doReturn(false).when(temporaryFile).exists();

        DownloadAction action = spy(new DownloadAction(null));
        action.download(client, TEST_URL, temporaryFile);

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
        HttpClient client = mockHttpClient(404, "");

        File temporaryFile = mock(File.class);
        doReturn(false).when(temporaryFile).exists();

        DownloadAction action = spy(new DownloadAction(null));
        action.download(client, TEST_URL, temporaryFile);

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

    /**
     * Scenario: Pretend a partially downloaded file already exists.
     *
     * Verify that:
     *  * Range header is set in request
     *  * Content will be appended to existing file
     *  * Content will be marked as downloaded in catalog
     */
    @Test
    public void testResumingDownloadFromExistingFile() throws Exception {
        DownloadContent content = new DownloadContent.Builder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(4223)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File temporaryFile = mockTemporaryFile(true, 1337L);
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        doReturn(outputStream).when(action).openFile(eq(temporaryFile), anyBoolean());

        HttpClient client = mockHttpClient(HttpStatus.SC_PARTIAL_CONTENT, "HelloWorld");
        doReturn(client).when(action).buildHttpClient();

        File destinationFile = mock(File.class);
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        doReturn(true).when(action).verify(eq(temporaryFile), anyString());
        doNothing().when(action).extract(eq(temporaryFile), eq(destinationFile), anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        ArgumentCaptor<HttpGet> argument = ArgumentCaptor.forClass(HttpGet.class);
        verify(client).execute(argument.capture());

        HttpGet request = argument.getValue();
        Assert.assertTrue(request.containsHeader("Range"));
        Assert.assertEquals("bytes=1337-", request.getFirstHeader("Range").getValue());
        Assert.assertEquals("HelloWorld", new String(outputStream.toByteArray(), "UTF-8"));

        verify(action).openFile(eq(temporaryFile), eq(true));
        verify(catalog).markAsDownloaded(content);
        verify(temporaryFile).delete();
    }

    /**
     * Scenario: Download fails with IOException.
     *
     * Verify that:
     *  * Partially downloaded file will not be deleted
     *  * Content will not be marked as downloaded in catalog
     */
    @Test
    public void testTemporaryFileIsNotDeletedAfterDownloadAborted() throws Exception {
        DownloadContent content = new DownloadContent.Builder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(4223)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File temporaryFile = mockTemporaryFile(true, 1337L);
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        ByteArrayOutputStream outputStream = spy(new ByteArrayOutputStream());
        doReturn(outputStream).when(action).openFile(eq(temporaryFile), anyBoolean());
        doThrow(IOException.class).when(outputStream).write(any(byte[].class), anyInt(), anyInt());

        HttpClient client = mockHttpClient(HttpStatus.SC_PARTIAL_CONTENT, "HelloWorld");
        doReturn(client).when(action).buildHttpClient();

        File destinationFile = mock(File.class);
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog, never()).markAsDownloaded(content);
        verify(temporaryFile, never()).delete();
    }

    /**
     * Scenario: Partially downloaded file is already complete.
     *
     * Verify that:
     *  * No download request is made
     *  * File is treated as completed and will be verified and extracted
     *  * Content is marked as downloaded in catalog
     */
    @Test
    public void testNoRequestIsSentIfFileIsAlreadyComplete() throws Exception {
        DownloadContent content = new DownloadContent.Builder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(1337L)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File temporaryFile = mockTemporaryFile(true, 1337L);
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        File destinationFile = mock(File.class);
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        doReturn(true).when(action).verify(eq(temporaryFile), anyString());
        doNothing().when(action).extract(eq(temporaryFile), eq(destinationFile), anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        verify(action, never()).download(any(HttpClient.class), anyString(), eq(temporaryFile));
        verify(action).verify(eq(temporaryFile), anyString());
        verify(action).extract(eq(temporaryFile), eq(destinationFile), anyString());
        verify(catalog).markAsDownloaded(content);
    }

    /**
     * Scenario: Download is completed but verification (checksum) failed.
     *
     * Verify that:
     *  * Downloaded file is deleted
     *  * File will not be extracted
     *  * Content is not marked as downloaded in the catalog
     */
    @Test
    public void testTemporaryFileWillBeDeletedIfVerificationFails() throws Exception {
        DownloadContent content = new DownloadContent.Builder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(1337L)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);
        doNothing().when(action).download(any(HttpClient.class), anyString(), any(File.class));
        doReturn(false).when(action).verify(any(File.class), anyString());

        File temporaryFile = mockTemporaryFile(true, 1337L);
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        File destinationFile = mock(File.class);
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        action.perform(RuntimeEnvironment.application, catalog);

        verify(temporaryFile).delete();
        verify(action, never()).extract(any(File.class), any(File.class), anyString());
        verify(catalog, never()).markAsDownloaded(content);
    }

    private static File mockTemporaryFile(boolean exists, long length) {
        File temporaryFile = mock(File.class);
        doReturn(exists).when(temporaryFile).exists();
        doReturn(length).when(temporaryFile).length();
        return temporaryFile;
    }

    private static HttpClient mockHttpClient(int statusCode, String content) throws Exception {
        StatusLine status = mock(StatusLine.class);
        doReturn(statusCode).when(status).getStatusCode();

        HttpEntity entity = mock(HttpEntity.class);
        doReturn(new ByteArrayInputStream(content.getBytes("UTF-8"))).when(entity).getContent();

        HttpResponse response = mock(HttpResponse.class);
        doReturn(status).when(response).getStatusLine();
        doReturn(entity).when(response).getEntity();

        HttpClient client = mock(HttpClient.class);
        doReturn(response).when(client).execute(any(HttpUriRequest.class));

        return client;
    }
}
