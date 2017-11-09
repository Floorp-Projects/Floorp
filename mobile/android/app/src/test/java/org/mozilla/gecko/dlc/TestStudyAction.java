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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

/**
 * StudyAction: Scan the catalog for "new" content available for download.
 */
@RunWith(TestRunner.class)
public class TestStudyAction {
    /**
     * Scenario: Catalog is empty.
     *
     * Verify that:
     *  * No download is scheduled
     *  * Download action is not started
     */
    @Test
    public void testPerformWithEmptyCatalog() {
        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        when(catalog.getContentToStudy()).thenReturn(new ArrayList<DownloadContent>());

        StudyAction action = spy(new StudyAction());
        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).getContentToStudy();
        verify(catalog, never()).markAsDownloaded(any(DownloadContent.class));
        verify(action, never()).startDownloads(any(Context.class));
    }

    /**
     * Scenario: Catalog contains two items that have not been downloaded yet.
     *
     * Verify that:
     *  * Both items are scheduled to be downloaded
     */
    @Test
    public void testPerformWithNewContent() {
        DownloadContent content1 = new DownloadContentBuilder()
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setKind(DownloadContent.KIND_FONT)
                .build();
        DownloadContent content2 = new DownloadContentBuilder()
                .setType(DownloadContent.TYPE_ASSET_ARCHIVE)
                .setKind(DownloadContent.KIND_FONT)
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        when(catalog.getContentToStudy()).thenReturn(Arrays.asList(content1, content2));

        StudyAction action = spy(new StudyAction());
        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog).scheduleDownload(content1);
        verify(catalog).scheduleDownload(content2);
    }

    /**
     * Scenario: Catalog contains item that are scheduled for download.
     *
     * Verify that:
     *  * Download action is started
     */
    @Test
    public void testStartingDownloadsAfterScheduling() {
        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        when(catalog.hasScheduledDownloads()).thenReturn(true);

        StudyAction action = spy(new StudyAction());
        action.perform(RuntimeEnvironment.application, catalog);

        verify(action).startDownloads(any(Context.class));
    }

    /**
     * Scenario: Catalog contains unknown content.
     *
     * Verify that:
     *  * Unknown content is not scheduled for download.
     */
    @Test
    public void testPerformWithUnknownContent() {
        DownloadContent content = new DownloadContentBuilder()
                .setType("Unknown-Type")
                .setKind("Unknown-Kind")
                .build();

        DownloadContentCatalog catalog = mock(DownloadContentCatalog.class);
        when(catalog.getContentToStudy()).thenReturn(Collections.singletonList(content));

        StudyAction action = spy(new StudyAction());
        action.perform(RuntimeEnvironment.application, catalog);

        verify(catalog, never()).scheduleDownload(content);
    }
}
