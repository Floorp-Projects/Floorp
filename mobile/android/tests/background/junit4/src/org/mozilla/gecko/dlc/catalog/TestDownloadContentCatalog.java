/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc.catalog;

import android.util.AtomicFile;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.util.Arrays;
import java.util.Collections;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

@RunWith(TestRunner.class)
public class TestDownloadContentCatalog {
    /**
     * Scenario: Create a new, fresh catalog.
     *
     * Verify that:
     *  * Catalog has not changed
     *  * Unchanged catalog will not be saved to disk
     */
    @Test
    public void testUntouchedCatalogHasNotChangedAndWillNotBePersisted() throws Exception {
        AtomicFile file = mock(AtomicFile.class);
        doReturn("{content:[]}".getBytes("UTF-8")).when(file).readFully();

        DownloadContentCatalog catalog = spy(new DownloadContentCatalog(file));
        catalog.loadFromDisk();

        Assert.assertFalse("Catalog has not changed", catalog.hasCatalogChanged());

        catalog.writeToDisk();

        Assert.assertFalse("Catalog has not changed", catalog.hasCatalogChanged());

        verify(file, never()).startWrite();
    }

    /**
     * Scenario: Create a new, fresh catalog.
     *
     * Verify that:
     *  * Catalog is bootstrapped with items.
     */
    @Test
    public void testCatalogIsBootstrappedIfFileDoesNotExist() throws Exception {
        // The catalog is only bootstrapped if fonts are excluded from the build. If this is a build
        // with fonts included then ignore this test.
        Assume.assumeTrue("Fonts are excluded from build", AppConstants.MOZ_ANDROID_EXCLUDE_FONTS);

        AtomicFile file = mock(AtomicFile.class);
        doThrow(FileNotFoundException.class).when(file).readFully();

        DownloadContentCatalog catalog = spy(new DownloadContentCatalog(file));
        catalog.loadFromDisk();

        Assert.assertTrue("Catalog is not empty", catalog.getContentWithoutState().size() > 0);
    }

    /**
     * Scenario: Schedule downloading an item from the catalog.
     *
     * Verify that:
     *  * Catalog has changed
     */
    @Test
    public void testCatalogHasChangedWhenDownloadIsScheduled() throws Exception {
        DownloadContentCatalog catalog = spy(new DownloadContentCatalog(mock(AtomicFile.class)));
        DownloadContent content = new DownloadContent.Builder().build();
        catalog.onCatalogLoaded(Collections.singletonList(content));

        Assert.assertFalse("Catalog has not changed", catalog.hasCatalogChanged());

        catalog.scheduleDownload(content);

        Assert.assertTrue("Catalog has changed", catalog.hasCatalogChanged());
    }

    /**
     * Scenario: Mark an item in the catalog as downloaded.
     *
     * Verify that:
     *  * Catalog has changed
     */
    @Test
    public void testCatalogHasChangedWhenContentIsDownloaded() throws Exception {
        DownloadContentCatalog catalog = spy(new DownloadContentCatalog(mock(AtomicFile.class)));
        DownloadContent content = new DownloadContent.Builder().build();
        catalog.onCatalogLoaded(Collections.singletonList(content));

        Assert.assertFalse("Catalog has not changed", catalog.hasCatalogChanged());

        catalog.markAsDownloaded(content);

        Assert.assertTrue("Catalog has changed", catalog.hasCatalogChanged());
    }

    /**
     * Scenario: Mark an item in the catalog as permanently failed.
     *
     * Verify that:
     *  * Catalog has changed
     */
    @Test
    public void testCatalogHasChangedIfDownloadHasFailedPermanently() throws Exception {
        DownloadContentCatalog catalog = spy(new DownloadContentCatalog(mock(AtomicFile.class)));
        DownloadContent content = new DownloadContent.Builder().build();
        catalog.onCatalogLoaded(Collections.singletonList(content));

        Assert.assertFalse("Catalog has not changed", catalog.hasCatalogChanged());

        catalog.markAsPermanentlyFailed(content);

        Assert.assertTrue("Catalog has changed", catalog.hasCatalogChanged());
    }

    /**
     * Scenario: Mark an item in the catalog as ignored.
     *
     * Verify that:
     *  * Catalog has changed
     */
    @Test
    public void testCatalogHasChangedIfContentIsIgnored() throws Exception {
        DownloadContentCatalog catalog = spy(new DownloadContentCatalog(mock(AtomicFile.class)));
        DownloadContent content = new DownloadContent.Builder().build();
        catalog.onCatalogLoaded(Collections.singletonList(content));

        Assert.assertFalse("Catalog has not changed", catalog.hasCatalogChanged());

        catalog.markAsIgnored(content);

        Assert.assertTrue("Catalog has changed", catalog.hasCatalogChanged());
    }

    /**
     * Scenario: A changed catalog is written to disk.
     *
     * Verify that:
     *  * Before write: Catalog has changed
     *  * After write: Catalog has not changed.
     */
    @Test
    public void testCatalogHasNotChangedAfterWritingToDisk() throws Exception {
        AtomicFile file = mock(AtomicFile.class);
        doReturn(mock(FileOutputStream.class)).when(file).startWrite();

        DownloadContentCatalog catalog = spy(new DownloadContentCatalog(file));
        DownloadContent content = new DownloadContent.Builder().build();
        catalog.onCatalogLoaded(Collections.singletonList(content));

        catalog.scheduleDownload(content);

        Assert.assertTrue("Catalog has changed", catalog.hasCatalogChanged());

        catalog.writeToDisk();

        Assert.assertFalse("Catalog has not changed", catalog.hasCatalogChanged());
    }

    /**
     * Scenario: A catalog with multiple items in different states.
     *
     * Verify that:
     *  * getContentWithoutState(), getDownloadedContent() and getScheduledDownloads() returns
     *    the correct items depenending on their state.
     */
    @Test
    public void testContentClassification() {
        DownloadContentCatalog catalog = spy(new DownloadContentCatalog(mock(AtomicFile.class)));

        DownloadContent content1 = new DownloadContent.Builder().setState(DownloadContent.STATE_NONE).build();
        DownloadContent content2 = new DownloadContent.Builder().setState(DownloadContent.STATE_NONE).build();
        DownloadContent content3 = new DownloadContent.Builder().setState(DownloadContent.STATE_SCHEDULED).build();
        DownloadContent content4 = new DownloadContent.Builder().setState(DownloadContent.STATE_SCHEDULED).build();
        DownloadContent content5 = new DownloadContent.Builder().setState(DownloadContent.STATE_SCHEDULED).build();
        DownloadContent content6 = new DownloadContent.Builder().setState(DownloadContent.STATE_DOWNLOADED).build();
        DownloadContent content7 = new DownloadContent.Builder().setState(DownloadContent.STATE_FAILED).build();
        DownloadContent content8 = new DownloadContent.Builder().setState(DownloadContent.STATE_IGNORED).build();
        DownloadContent content9 = new DownloadContent.Builder().setState(DownloadContent.STATE_IGNORED).build();


        catalog.onCatalogLoaded(Arrays.asList(content1, content2, content3, content4, content5, content6,
                                              content7, content8, content9));

        Assert.assertEquals(2, catalog.getContentWithoutState().size());
        Assert.assertEquals(1, catalog.getDownloadedContent().size());
        Assert.assertEquals(3, catalog.getScheduledDownloads().size());

        Assert.assertTrue(catalog.getContentWithoutState().contains(content1));
        Assert.assertTrue(catalog.getContentWithoutState().contains(content2));

        Assert.assertTrue(catalog.getDownloadedContent().contains(content6));

        Assert.assertTrue(catalog.getScheduledDownloads().contains(content3));
        Assert.assertTrue(catalog.getScheduledDownloads().contains(content4));
        Assert.assertTrue(catalog.getScheduledDownloads().contains(content5));
    }

    /**
     * Scenario: Calling rememberFailure() on a catalog with varying values
     */
    @Test
    public void testRememberingFailures() {
        DownloadContentCatalog catalog = new DownloadContentCatalog(mock(AtomicFile.class));
        Assert.assertFalse(catalog.hasCatalogChanged());

        DownloadContent content = new DownloadContent.Builder().build();
        Assert.assertEquals(0, content.getFailures());

        catalog.rememberFailure(content, 42);
        Assert.assertEquals(1, content.getFailures());
        Assert.assertTrue(catalog.hasCatalogChanged());

        catalog.rememberFailure(content, 42);
        Assert.assertEquals(2, content.getFailures());

        // Failure counter is reset if different failure has been reported
        catalog.rememberFailure(content, 23);
        Assert.assertEquals(1, content.getFailures());

        // Failure counter is reset after successful download
        catalog.markAsDownloaded(content);
        Assert.assertEquals(0, content.getFailures());
    }

    /**
     * Scenario: Content has failed multiple times with the same failure type.
     *
     * Verify that:
     *  * Content is marked as permanently failed
     */
    @Test
    public void testContentWillBeMarkedAsPermanentlyFailedAfterMultipleFailures() {
        DownloadContentCatalog catalog = new DownloadContentCatalog(mock(AtomicFile.class));

        DownloadContent content = new DownloadContent.Builder().build();
        Assert.assertEquals(DownloadContent.STATE_NONE, content.getState());

        for (int i = 0; i < 10; i++) {
            catalog.rememberFailure(content, 42);

            Assert.assertEquals(i + 1, content.getFailures());
            Assert.assertEquals(DownloadContent.STATE_NONE, content.getState());
        }

        catalog.rememberFailure(content, 42);
        Assert.assertEquals(10, content.getFailures());
        Assert.assertEquals(DownloadContent.STATE_FAILED, content.getState());
    }
}
