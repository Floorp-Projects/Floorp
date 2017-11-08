/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;
import android.support.v4.util.ArrayMap;
import android.support.v4.util.AtomicFile;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentBuilder;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.mozilla.gecko.util.IOUtils;
import org.robolectric.RuntimeEnvironment;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.List;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

/**
 * SyncAction: Synchronize catalog from a (mocked) Kinto instance.
 */
@RunWith(TestRunner.class)
public class TestSyncAction {
    /**
     * Scenario: The server returns an empty record set.
     */
    @Test
    public void testEmptyResult() throws Exception {
        SyncAction action = spy(new SyncAction());
        doReturn(true).when(action).isSyncEnabledForClient(RuntimeEnvironment.application);
        doReturn(new JSONArray()).when(action).fetchRawCatalog(anyLong());

        action.perform(RuntimeEnvironment.application, mockCatalog());

        verify(action, never()).createContent(anyCatalog(), anyJSONObject());
        verify(action, never()).updateContent(anyCatalog(), anyJSONObject(), anyContent());
        verify(action, never()).deleteContent(anyCatalog(), anyString());

        verify(action, never()).startStudyAction(anyContext());
    }

    /**
     * Scenario: The server returns an item that is not in the catalog yet.
     */
    @Test
    public void testAddingNewContent() throws Exception {
        SyncAction action = spy(new SyncAction());
        doReturn(true).when(action).isSyncEnabledForClient(RuntimeEnvironment.application);
        doReturn(fromFile("dlc_sync_single_font.json")).when(action).fetchRawCatalog(anyLong());

        DownloadContentCatalog catalog = mockCatalog();

        action.perform(RuntimeEnvironment.application, catalog);

        // A new content item has been created
        verify(action).createContent(anyCatalog(), anyJSONObject());

        // No content item has been updated or deleted
        verify(action, never()).updateContent(anyCatalog(), anyJSONObject(), anyContent());
        verify(action, never()).deleteContent(anyCatalog(), anyString());

        // A new item has been added to the catalog
        ArgumentCaptor<DownloadContent> captor = ArgumentCaptor.forClass(DownloadContent.class);
        verify(catalog).add(captor.capture());

        // The item matches the values from the server response
        DownloadContent content = captor.getValue();
        Assert.assertEquals("c906275c-3747-fe27-426f-6187526a6f06", content.getId());
        Assert.assertEquals("4ed509317f1bb441b185ea13bf1c9d19d1a0b396962efa3b5dc3190ad88f2067", content.getChecksum());
        Assert.assertEquals("960be4fc5a92c1dc488582b215d5d75429fd4ffbee463105d29992cd792a912e", content.getDownloadChecksum());
        Assert.assertEquals("CharisSILCompact-R.ttf", content.getFilename());
        Assert.assertEquals(DownloadContent.KIND_FONT, content.getKind());
        Assert.assertEquals("/attachments/0d28a72d-a51f-46f8-9e5a-f95c61de904e.gz", content.getLocation());
        Assert.assertEquals(DownloadContent.TYPE_ASSET_ARCHIVE, content.getType());
        Assert.assertEquals(1455710632607L, content.getLastModified());
        Assert.assertEquals(1727656L, content.getSize());
        Assert.assertEquals(DownloadContent.STATE_NONE, content.getState());
    }

    /**
     * Scenario: The catalog is using the old format, we want to make sure we abort cleanly.
     */
    @Test
    public void testUpdatingWithOldCatalog() throws Exception{
        SyncAction action = spy(new SyncAction());
        doReturn(true).when(action).isSyncEnabledForClient(RuntimeEnvironment.application);
        doReturn(fromFile("dlc_sync_old_format.json")).when(action).fetchRawCatalog(anyLong());

        DownloadContent existingContent = createTestContent("c906275c-3747-fe27-426f-6187526a6f06");
        DownloadContentCatalog catalog = spy(new MockedContentCatalog(existingContent));

        action.perform(RuntimeEnvironment.application, catalog);

        // make sure nothing was done
        verify(action, never()).createContent(anyCatalog(), anyJSONObject());
        verify(action, never()).updateContent(anyCatalog(), anyJSONObject(), anyContent());
        verify(action, never()).deleteContent(anyCatalog(), anyString());
        verify(action, never()).startStudyAction(anyContext());
    }


    /**
     * Scenario: The catalog contains one item and the server returns a new version.
     */
    @Test
    public void testUpdatingExistingContent() throws Exception{
        SyncAction action = spy(new SyncAction());
        doReturn(true).when(action).isSyncEnabledForClient(RuntimeEnvironment.application);
        doReturn(fromFile("dlc_sync_single_font.json")).when(action).fetchRawCatalog(anyLong());

        DownloadContent existingContent = createTestContent("c906275c-3747-fe27-426f-6187526a6f06");
        DownloadContentCatalog catalog = spy(new MockedContentCatalog(existingContent));

        action.perform(RuntimeEnvironment.application, catalog);

        // A content item has been updated
        verify(action).updateContent(anyCatalog(), anyJSONObject(), eq(existingContent));

        // No content item has been created or deleted
        verify(action, never()).createContent(anyCatalog(), anyJSONObject());
        verify(action, never()).deleteContent(anyCatalog(), anyString());

        // An item has been updated in the catalog
        ArgumentCaptor<DownloadContent> captor = ArgumentCaptor.forClass(DownloadContent.class);
        verify(catalog).update(captor.capture());

        // The item has the new values from the sever response
        DownloadContent content = captor.getValue();
        Assert.assertEquals("c906275c-3747-fe27-426f-6187526a6f06", content.getId());
        Assert.assertEquals("4ed509317f1bb441b185ea13bf1c9d19d1a0b396962efa3b5dc3190ad88f2067", content.getChecksum());
        Assert.assertEquals("960be4fc5a92c1dc488582b215d5d75429fd4ffbee463105d29992cd792a912e", content.getDownloadChecksum());
        Assert.assertEquals("CharisSILCompact-R.ttf", content.getFilename());
        Assert.assertEquals(DownloadContent.KIND_FONT, content.getKind());
        Assert.assertEquals("/attachments/0d28a72d-a51f-46f8-9e5a-f95c61de904e.gz", content.getLocation());
        Assert.assertEquals(DownloadContent.TYPE_ASSET_ARCHIVE, content.getType());
        Assert.assertEquals(1455710632607L, content.getLastModified());
        Assert.assertEquals(1727656L, content.getSize());
        Assert.assertEquals(DownloadContent.STATE_UPDATED, content.getState());
    }

    /**
     * Scenario: Catalog contains one item and the server returns that it has been deleted.
     */
    @Test
    public void testDeletingExistingContent() throws Exception {
        SyncAction action = spy(new SyncAction());
        doReturn(true).when(action).isSyncEnabledForClient(RuntimeEnvironment.application);
        doReturn(fromFile("dlc_sync_deleted_item.json")).when(action).fetchRawCatalog(anyLong());

        final String id = "c906275c-3747-fe27-426f-6187526a6f06";
        DownloadContent existingContent = createTestContent(id);
        DownloadContentCatalog catalog = spy(new MockedContentCatalog(existingContent));

        action.perform(RuntimeEnvironment.application, catalog);

        // A content item has been deleted
        verify(action).deleteContent(anyCatalog(), eq(id));

        // No content item has been created or updated
        verify(action, never()).createContent(anyCatalog(), anyJSONObject());
        verify(action, never()).updateContent(anyCatalog(), anyJSONObject(), anyContent());

        // An item has been marked for deletion in the catalog
        ArgumentCaptor<DownloadContent> captor = ArgumentCaptor.forClass(DownloadContent.class);
        verify(catalog).markAsDeleted(captor.capture());

        DownloadContent content = captor.getValue();
        Assert.assertEquals(id, content.getId());

        List<DownloadContent> contentToDelete = catalog.getContentToDelete();
        Assert.assertEquals(1, contentToDelete.size());
        Assert.assertEquals(id, contentToDelete.get(0).getId());
    }

    /**
     * Create a DownloadContent object with arbitrary data.
     */
    private DownloadContent createTestContent(String id) {
        return new DownloadContentBuilder()
                .setId(id)
                .setLocation("/somewhere/something")
                .setFilename("some.file")
                .setChecksum("Some-checksum")
                .setDownloadChecksum("Some-download-checksum")
                .setLastModified(4223)
                .setType("Some-type")
                .setKind("Some-kind")
                .setSize(27)
                .setState(DownloadContent.STATE_SCHEDULED)
                .build();
    }

    /**
     * Create a Kinto response from a JSON file.
     */
    private JSONArray fromFile(String fileName) throws IOException, JSONException {
        URL url = getClass().getResource("/" + fileName);
        if (url == null) {
            throw new FileNotFoundException(fileName);
        }

        InputStream inputStream = null;
        ByteArrayOutputStream  outputStream = null;

        try {
            inputStream = new BufferedInputStream(new FileInputStream(url.getPath()));
            outputStream = new ByteArrayOutputStream();

            IOUtils.copy(inputStream, outputStream);

            JSONObject object = new JSONObject(outputStream.toString());

            return object.getJSONArray("data");
        } finally {
            IOUtils.safeStreamClose(inputStream);
            IOUtils.safeStreamClose(outputStream);
        }
    }

    private static class MockedContentCatalog extends DownloadContentCatalog {
        public MockedContentCatalog(DownloadContent content) {
            super(mock(AtomicFile.class));

            ArrayMap<String, DownloadContent> map = new ArrayMap<>();
            map.put(content.getId(), content);

            onCatalogLoaded(map);
        }
    }

    private DownloadContentCatalog mockCatalog() {
        return mock(DownloadContentCatalog.class);
    }

    private DownloadContentCatalog anyCatalog() {
        return any(DownloadContentCatalog.class);
    }

    private JSONObject anyJSONObject() {
        return any(JSONObject.class);
    }

    private DownloadContent anyContent() {
        return any(DownloadContent.class);
    }

    private Context anyContext() {
        return any(Context.class);
    }
}
