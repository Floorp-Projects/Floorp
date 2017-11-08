/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.content.Context;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.dlc.catalog.DownloadContent;
import org.mozilla.gecko.dlc.catalog.DownloadContentBuilder;
import org.mozilla.gecko.dlc.catalog.DownloadContentCatalog;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.io.File;

import dalvik.annotation.TestTarget;
import edu.emory.mathcs.backport.java.util.Collections;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class TestCleanupAction {
    @Test
    public void testEmptyCatalog() {
        final DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.emptyList()).when(catalog).getContentToDelete();

        final CleanupAction action = spy(new CleanupAction());
        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).getContentToDelete();
        verify(action, never()).cleanupContent(any(Context.class), any(DownloadContentCatalog.class), any(DownloadContent.class));
    }

    @Test
    public void testWithUnknownType() {
        final DownloadContent content = new DownloadContentBuilder()
                .setType("RandomType")
                .build();

        final DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getContentToDelete();

        final CleanupAction action = spy(new CleanupAction());
        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).getContentToDelete();
        verify(action, never()).cleanupContent(any(Context.class), any(DownloadContentCatalog.class), any(DownloadContent.class));
    }

    @SuppressWarnings("ResultOfMethodCallIgnored")
    @Test
    public void testWithContentThatDoesNotExistAnymore() throws Exception {
        final DownloadContent content = new DownloadContentBuilder()
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .build();

        final DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getContentToDelete();

        final File file = mock(File.class);
        doReturn(false).when(file).exists();

        final CleanupAction action = spy(new CleanupAction());
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).getContentToDelete();
        verify(action).cleanupContent(RuntimeEnvironment.application, catalog, content);
        verify(file).exists();
        verify(file, never()).delete();
        verify(catalog).remove(content);
    }

    @Test
    public void testWithDeletableContent() throws Exception {
        final DownloadContent content = new DownloadContentBuilder()
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .build();

        final DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getContentToDelete();

        final File file = mock(File.class);
        doReturn(true).when(file).exists();
        doReturn(true).when(file).delete();

        final CleanupAction action = spy(new CleanupAction());
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).getContentToDelete();
        verify(action).cleanupContent(RuntimeEnvironment.application, catalog, content);
        verify(file).exists();
        verify(file).delete();
        verify(catalog).remove(content);
    }

    @Test
    public void testWithFileThatCannotBeDeleted() throws Exception {
        final DownloadContent content = new DownloadContentBuilder()
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .build();

        final DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        doReturn(Collections.singletonList(content)).when(catalog).getContentToDelete();

        final File file = mock(File.class);
        doReturn(true).when(file).exists();
        doReturn(false).when(file).delete();

        final CleanupAction action = spy(new CleanupAction());
        doReturn(file).when(action).getDestinationFile(RuntimeEnvironment.application, content);

        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).getContentToDelete();
        verify(action).cleanupContent(RuntimeEnvironment.application, catalog, content);
        verify(file).exists();
        verify(file).delete();
        verify(catalog, never()).remove(content);
    }
}
