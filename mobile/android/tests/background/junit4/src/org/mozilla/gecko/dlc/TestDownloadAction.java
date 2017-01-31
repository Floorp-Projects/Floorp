/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentBuilder;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.robolectric.RuntimeEnvironment;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.util.Arrays;
import java.util.Collections;

import static org.mockito.Matchers.*;
import static org.mockito.Mockito.*;

/**
 * DownloadAction: Download content that has been scheduled during "study" or "verify".
 */
@RunWith(TestRunner.class)
public class TestDownloadAction {
    private static final String TEST_URL = "http://example.org";

    private static final int STATUS_OK = 200;
    private static final int STATUS_PARTIAL_CONTENT = 206;

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

        verify(action, never()).buildHttpURLConnection(anyString());
        verify(action, never()).download(anyString(), any(File.class));
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
        verify(action, never()).buildHttpURLConnection(anyString());
        verify(action, never()).download(anyString(), any(File.class));
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
        DownloadContent content = new DownloadContentBuilder().build();

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

        verify(action, never()).download(anyString(), any(File.class));
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
        HttpURLConnection connection = mockHttpURLConnection(500, "");

        File temporaryFile = mock(File.class);
        doReturn(false).when(temporaryFile).exists();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(connection).when(action).buildHttpURLConnection(anyString());
        action.download(TEST_URL, temporaryFile);

        verify(connection).getInputStream();
    }

    /**
     * Scenario: Server returns a client error (HTTP 404).
     *
     * Verify that:
     *  * Situation is treated as unrecoverable (UnrecoverableDownloadContentException)
     */
    @Test(expected=BaseAction.UnrecoverableDownloadContentException.class)
    public void testClientErrorsAreUnrecoverable() throws Exception {
        HttpURLConnection connection = mockHttpURLConnection(404, "");

        File temporaryFile = mock(File.class);
        doReturn(false).when(temporaryFile).exists();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(connection).when(action).buildHttpURLConnection(anyString());
        action.download(TEST_URL, temporaryFile);

        verify(connection).getInputStream();
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
        DownloadContent content = new DownloadContentBuilder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File file = mockNotExistingFile();
        doReturn(file).when(action).createTemporaryFile(RuntimeEnvironment.application, content);
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        doReturn(false).when(action).verify(eq(file), anyString());
        doNothing().when(action).download(anyString(), eq(file));
        doReturn(true).when(action).verify(eq(file), anyString());
        doNothing().when(action).extract(eq(file), eq(file), anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        verify(action).download(anyString(), eq(file));
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
        DownloadContent content = new DownloadContentBuilder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(4223)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File temporaryFile = mockFileWithSize(1337L);
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        doReturn(outputStream).when(action).openFile(eq(temporaryFile), anyBoolean());

        HttpURLConnection connection = mockHttpURLConnection(STATUS_PARTIAL_CONTENT, "HelloWorld");
        doReturn(connection).when(action).buildHttpURLConnection(anyString());

        File destinationFile = mockNotExistingFile();
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        doReturn(true).when(action).verify(eq(temporaryFile), anyString());
        doNothing().when(action).extract(eq(temporaryFile), eq(destinationFile), anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        verify(connection).getInputStream();
        verify(connection).setRequestProperty("Range", "bytes=1337-");

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
        DownloadContent content = new DownloadContentBuilder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(4223)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File temporaryFile = mockFileWithSize(1337L);
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        ByteArrayOutputStream outputStream = spy(new ByteArrayOutputStream());
        doReturn(outputStream).when(action).openFile(eq(temporaryFile), anyBoolean());
        doThrow(IOException.class).when(outputStream).write(any(byte[].class), anyInt(), anyInt());

        HttpURLConnection connection = mockHttpURLConnection(STATUS_PARTIAL_CONTENT, "HelloWorld");
        doReturn(connection).when(action).buildHttpURLConnection(anyString());

        doReturn(mockNotExistingFile()).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog, never()).markAsDownloaded(content);
        verify(action, never()).verify(any(File.class), anyString());
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
        DownloadContent content = new DownloadContentBuilder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(1337L)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);

        File temporaryFile = mockFileWithSize(1337L);
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        File destinationFile = mockNotExistingFile();
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        doReturn(true).when(action).verify(eq(temporaryFile), anyString());
        doNothing().when(action).extract(eq(temporaryFile), eq(destinationFile), anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        verify(action, never()).download(anyString(), eq(temporaryFile));
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
        DownloadContent content = new DownloadContentBuilder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(1337L)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getScheduledDownloads();

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);
        doNothing().when(action).download(anyString(), any(File.class));
        doReturn(false).when(action).verify(any(File.class), anyString());

        File temporaryFile = mockNotExistingFile();
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        File destinationFile = mockNotExistingFile();
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        action.perform(RuntimeEnvironment.application, catalog);

        verify(temporaryFile).delete();
        verify(action, never()).extract(any(File.class), any(File.class), anyString());
        verify(catalog, never()).markAsDownloaded(content);
    }

    /**
     * Scenario: Not enough storage space for content is available.
     *
     * Verify that:
     *  * No download will per performed
     */
    @Test
    public void testNoDownloadIsPerformedIfNotEnoughStorageIsAvailable() throws Exception {
        DownloadContent content = createFontWithSize(1337L);
        DownloadContentCatalog catalog = mockCatalogWithScheduledDownloads(content);

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);
        doReturn(true).when(action).isConnectedToNetwork(RuntimeEnvironment.application);

        File temporaryFile = mockNotExistingFile();
        doReturn(temporaryFile).when(action).createTemporaryFile(RuntimeEnvironment.application, content);

        File destinationFile = mockNotExistingFile();
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        doReturn(true).when(action).hasEnoughDiskSpace(content, destinationFile, temporaryFile);

        verify(action, never()).buildHttpURLConnection(anyString());
        verify(action, never()).download(anyString(), any(File.class));
        verify(action, never()).verify(any(File.class), anyString());
        verify(catalog, never()).markAsDownloaded(content);
    }

    /**
     * Scenario: Not enough storage space for temporary file available.
     *
     * Verify that:
     *  * hasEnoughDiskSpace() returns false
     */
    @Test
    public void testWithNotEnoughSpaceForTemporaryFile() throws Exception{
        DownloadContent content = createFontWithSize(2048);
        File destinationFile = mockNotExistingFile();
        File temporaryFile = mockNotExistingFileWithUsableSpace(1024);

        DownloadAction action = new DownloadAction(null);
        Assert.assertFalse(action.hasEnoughDiskSpace(content, destinationFile, temporaryFile));
    }

    /**
     * Scenario: Not enough storage space for destination file available.
     *
     * Verify that:
     *  * hasEnoughDiskSpace() returns false
     */
    @Test
    public void testWithNotEnoughSpaceForDestinationFile() throws Exception {
        DownloadContent content = createFontWithSize(2048);
        File destinationFile = mockNotExistingFileWithUsableSpace(1024);
        File temporaryFile = mockNotExistingFile();

        DownloadAction action = new DownloadAction(null);
        Assert.assertFalse(action.hasEnoughDiskSpace(content, destinationFile, temporaryFile));
    }

    /**
     * Scenario: Enough storage space for temporary and destination file available.
     *
     * Verify that:
     *  * hasEnoughDiskSpace() returns true
     */
    @Test
    public void testWithEnoughSpaceForEverything() throws Exception {
        DownloadContent content = createFontWithSize(2048);
        File destinationFile = mockNotExistingFileWithUsableSpace(4096);
        File temporaryFile = mockNotExistingFileWithUsableSpace(4096);

        DownloadAction action = new DownloadAction(null);
        Assert.assertTrue(action.hasEnoughDiskSpace(content, destinationFile, temporaryFile));
    }

    /**
     * Scenario: Download failed with network I/O error.
     *
     * Verify that:
     *  * Error is not counted as failure
     */
    @Test
    public void testNetworkErrorIsNotCountedAsFailure() throws Exception {
        DownloadContent content = createFont();
        DownloadContentCatalog catalog = mockCatalogWithScheduledDownloads(content);

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(true).when(action).isConnectedToNetwork(RuntimeEnvironment.application);
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);
        doReturn(mockNotExistingFile()).when(action).createTemporaryFile(RuntimeEnvironment.application, content);
        doReturn(mockNotExistingFile()).when(action).getDestinationFile(RuntimeEnvironment.application, content);
        doReturn(true).when(action).hasEnoughDiskSpace(eq(content), any(File.class), any(File.class));

        HttpURLConnection connection = mockHttpURLConnection(STATUS_OK, "");
        doThrow(IOException.class).when(connection).getInputStream();
        doReturn(connection).when(action).buildHttpURLConnection(anyString());

        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog, never()).rememberFailure(eq(content), anyInt());
        verify(catalog, never()).markAsDownloaded(content);
    }

    /**
     * Scenario: Disk IO Error when extracting file.
     *
     * Verify that:
     * * Error is counted as failure
     * * After multiple errors the content is marked as permanently failed
     */
    @Test
    public void testDiskIOErrorIsCountedAsFailure() throws Exception {
        DownloadContent content = createFont();
        DownloadContentCatalog catalog = mockCatalogWithScheduledDownloads(content);
        doCallRealMethod().when(catalog).rememberFailure(eq(content), anyInt());
        doCallRealMethod().when(catalog).markAsPermanentlyFailed(content);

        Assert.assertEquals(DownloadContent.STATE_NONE, content.getState());

        DownloadAction action = spy(new DownloadAction(null));
        doReturn(true).when(action).isConnectedToNetwork(RuntimeEnvironment.application);
        doReturn(false).when(action).isActiveNetworkMetered(RuntimeEnvironment.application);
        doReturn(mockNotExistingFile()).when(action).createTemporaryFile(RuntimeEnvironment.application, content);
        doReturn(mockNotExistingFile()).when(action).getDestinationFile(RuntimeEnvironment.application, content);
        doReturn(true).when(action).hasEnoughDiskSpace(eq(content), any(File.class), any(File.class));
        doNothing().when(action).download(anyString(), any(File.class));
        doReturn(true).when(action).verify(any(File.class), anyString());

        File destinationFile = mock(File.class);
        doReturn(false).when(destinationFile).exists();
        File parentFile = mock(File.class);
        doReturn(false).when(parentFile).mkdirs();
        doReturn(false).when(parentFile).exists();
        doReturn(parentFile).when(destinationFile).getParentFile();
        doReturn(destinationFile).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        for (int i = 0; i < 10; i++) {
            action.perform(RuntimeEnvironment.application, catalog);

            Assert.assertEquals(DownloadContent.STATE_NONE, content.getState());
        }

        action.perform(RuntimeEnvironment.application, catalog);

        Assert.assertEquals(DownloadContent.STATE_FAILED, content.getState());
        verify(catalog, times(11)).rememberFailure(eq(content), anyInt());
    }

    /**
     * Scenario: If the file to be downloaded is of kind - "hyphenation"
     *
     * Verify that:
     * * isHyphenationDictionary returns true for a download content with kind "hyphenation"
     * * isHyphenationDictionary returns false for a download content with unknown/different kind like  "Font"
     */
    @Test
    public void testIsHyphenationDictionary() throws Exception {
        DownloadContent hyphenationContent = createHyphenationDictionary();
        Assert.assertTrue(hyphenationContent.isHyphenationDictionary());
        DownloadContent fontContent = createFont();
        Assert.assertFalse(fontContent.isHyphenationDictionary());
        DownloadContent unknownContent = createUnknownContent(1024L);
        Assert.assertFalse(unknownContent.isHyphenationDictionary());
    }

    /**
     * Scenario: If the content to be downloaded is known
     *
     * Verify that:
     * * isKnownContent returns true for a downloadable content with a known kind and type.
     * * isKnownContent returns false for a downloadable content with unknown kind and type.
     */
    @Test
    public void testIsKnownContent() throws Exception {
        DownloadContent fontContent = createFontWithSize(1024L);
        DownloadContent hyphenationContent = createHyphenationDictionaryWithSize(1024L);
        DownloadContent unknownContent = createUnknownContent(1024L);
        DownloadContent contentWithUnknownType = createContentWithoutType(1024L);

        Assert.assertEquals(AppConstants.MOZ_ANDROID_EXCLUDE_FONTS, fontContent.isKnownContent());
        Assert.assertEquals(AppConstants.MOZ_EXCLUDE_HYPHENATION_DICTIONARIES, hyphenationContent.isKnownContent());

        Assert.assertFalse(unknownContent.isKnownContent());
        Assert.assertFalse(contentWithUnknownType.isKnownContent());
    }

    private DownloadContent createUnknownContent(long size) {
        return new DownloadContentBuilder()
                .setSize(size)
                .build();
    }

    private DownloadContent createContentWithoutType(long size) {
        return new DownloadContentBuilder()
                .setKind(DownloadContent.KIND_HYPHENATION_DICTIONARY)
                .setSize(size)
                .build();
    }

    private DownloadContent createFont() {
        return createFontWithSize(102400L);
    }

    private DownloadContent createFontWithSize(long size) {
        return new DownloadContentBuilder()
                .setKind(DownloadContent.KIND_FONT)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(size)
                .build();
    }

    private DownloadContent createHyphenationDictionary() {
        return createHyphenationDictionaryWithSize(102400L);
    }

    private DownloadContent createHyphenationDictionaryWithSize(long size) {
        return new DownloadContentBuilder()
                .setKind(DownloadContent.KIND_HYPHENATION_DICTIONARY)
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setSize(size)
                .build();
    }

    private DownloadContentCatalog mockCatalogWithScheduledDownloads(DownloadContent... content) {
        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Arrays.asList(content)).when(catalog).getScheduledDownloads();
        return catalog;
    }

    private static File mockNotExistingFile() {
        return mockFileWithUsableSpace(false, 0, Long.MAX_VALUE);
    }

    private static File mockNotExistingFileWithUsableSpace(long usableSpace) {
        return mockFileWithUsableSpace(false, 0, usableSpace);
    }

    private static File mockFileWithSize(long length) {
        return mockFileWithUsableSpace(true, length, Long.MAX_VALUE);
    }

    private static File mockFileWithUsableSpace(boolean exists, long length, long usableSpace) {
        File file = mock(File.class);
        doReturn(exists).when(file).exists();
        doReturn(length).when(file).length();

        File parentFile = mock(File.class);
        doReturn(usableSpace).when(parentFile).getUsableSpace();
        doReturn(parentFile).when(file).getParentFile();

        return file;
    }

    private static HttpURLConnection mockHttpURLConnection(int statusCode, String content) throws Exception {
        HttpURLConnection connection = mock(HttpURLConnection.class);

        doReturn(statusCode).when(connection).getResponseCode();
        doReturn(new ByteArrayInputStream(content.getBytes("UTF-8"))).when(connection).getInputStream();

        return connection;
    }
}
